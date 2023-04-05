#include "ya_getopt.h"
#include "scope_guard.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <inttypes.h>
#include <stdio.h>
#include <immintrin.h>
#include <emmintrin.h>


#define INFO(fmt, ...) fprintf(stdout, "I " fmt, ##__VA_ARGS__)
#define ERR(fmt, ...) fprintf(stderr, "E " fmt, ##__VA_ARGS__)


constexpr uint32_t          INDICES_BUFFER_SIZE_MAX     = (16 * 1024 * 1024);
constexpr uint32_t          INDICES_BUFFER_SIZE_DEFAULT = (512 * 1024);
constexpr uint32_t          TABLE_BUFFER_SIZE_MAX       = (1024 * 1024 * 1024);
constexpr uint32_t          TABLE_BUFFER_SIZE_DEFAULT   = TABLE_BUFFER_SIZE_MAX;
constexpr uint32_t          TABLE_ELEMENT_SIZE          = sizeof(uint16_t);
constexpr uint32_t          TABLE_INDEX_MASK_DEFAULT    = TABLE_BUFFER_SIZE_DEFAULT / TABLE_ELEMENT_SIZE - 1;
constexpr const char* const FILE_WITH_INDICES           = "indices.bin";
constexpr const char* const FILE_WITH_TABLE             = "table.bin";
constexpr uint16_t          TABLE_XOR_VAL               = 26849;
constexpr uint16_t          TABLE_ADD_VAL               = 41387;
constexpr uint32_t          INDEX_XOR_VAL               = (TABLE_XOR_VAL << 16) | TABLE_ADD_VAL;

struct config
{
	uint32_t indices_buffer_size = INDICES_BUFFER_SIZE_DEFAULT;

	uint32_t table_buffer_size = TABLE_BUFFER_SIZE_DEFAULT;

	char location_of_files[2048] = {};

	uint32_t table_index_mask = TABLE_INDEX_MASK_DEFAULT;

	uint32_t cycle_count = 1;

	uint32_t thread_count = 1;
};

struct thread_common_data
{
	uint32_t* const indices;

	const uint16_t* const table;

	const uint32_t count_of_input_indices;

	const uint32_t count_of_table_elements;

	thread_common_data(
			uint32_t* const       indices_rhs,
			const uint16_t* const table_rhs,
			const uint32_t        count_of_input_indices_rhs,
			const uint32_t        count_of_table_elements_rhs)
		: indices(indices_rhs)
		, table(table_rhs)
		, count_of_input_indices(count_of_input_indices_rhs)
		, count_of_table_elements(count_of_table_elements_rhs)
	{
	}
};

struct thread_data
{
	struct config*      conf = nullptr;

	thread_common_data* common_data = nullptr;

	uint32_t            id = 0;

	uint16_t            value = 0;

	uint64_t            table_accesses = 0;

	double              clock_sum = 0.0;
};

static void print_usage(const char *const progname)
{
	INFO("%s [-l <location_of_input_files>] [-i <indices_buffer_size>] [-t <table_buffer_size>] [-c <cycle_count>] [-d <thread_count>] [-h]\n",
			progname
			);
}

static uint32_t round_to_pow_of_two(unsigned int value)
{
	unsigned int   rounded_value = value;
	unsigned int   i;

	for (i = 0; i < 32; ++i)
	{
		if (rounded_value & 0x80000000)
		{
			break;
		}
		rounded_value <<= 1;
	}
	rounded_value = 1 << (31 - i);
	if (value > rounded_value)
	{
		rounded_value <<= 1;
	}

	return rounded_value;
}

template<typename T>
static uint32_t get_buffer_size(const char* const str_value)
{
	uint32_t value = (uint32_t)strtoul(str_value, nullptr, 10);
	return round_to_pow_of_two(value);
}

static int parse_args(int argc, char *argv[], struct config& conf)
{
	struct option longopts[] =
	{
		{
			/* name */ "location-of-files",
			/* has_arg */ya_required_argument,
			/* flag */nullptr,
			/* val */'l'
		},
		{
			/* name */ "indices-buffer-size",
			/* has_arg */ya_required_argument,
			/* flag */nullptr,
			/* val */'i'
		},
		{
			/* name */ "table-buffer-size",
			/* has_arg */ya_required_argument,
			/* flag */nullptr,
			/* val */'t'
		},
		{
			/* name */ "cycle-count",
			/* has_arg */ya_required_argument,
			/* flag */nullptr,
			/* val */'c'
		},
		{
			/* name */ "thread-count",
			/* has_arg */ya_required_argument,
			/* flag */nullptr,
			/* val */'d'
		},
		{
			/* name */ "help",
			/* has_arg */ya_no_argument,
			/* flag */nullptr,
			/* val */'h'
		},
		{
			/* name */ nullptr,
			/* has_arg */ya_no_argument,
			/* flag */nullptr,
			/* val */0
		}
	};

	ya_context ya_getopt_context;
	ya_context_initx(&ya_getopt_context);

	int longindex = 0;
	int optopt = 0;
	while ((optopt = ya_getopt_long(&ya_getopt_context, argc, argv, "l:i:t:c:d:a:b:e:gVh", longopts, &longindex)) != -1)
	{
		switch (optopt)
		{
			case 'l':
				strcpy(conf.location_of_files, ya_getopt_context.ya_optarg);
				break;

			case 'i':
				conf.indices_buffer_size = get_buffer_size<uint32_t>(ya_getopt_context.ya_optarg);
				break;

			case 't':
				conf.table_buffer_size = get_buffer_size<uint16_t>(ya_getopt_context.ya_optarg);
				break;

			case 'c':
				conf.cycle_count = (uint32_t)strtoul(ya_getopt_context.ya_optarg, nullptr, 10);
				break;

			case 'd':
				conf.thread_count = (uint32_t)strtoul(ya_getopt_context.ya_optarg, nullptr, 10);
				break;

			case 'h':
				print_usage(argv[0]);
				return -1;

			default:
				return -1;
		}
	}

	if (!conf.location_of_files[0])
	{
		ERR("location of files not given\n");
		return -1;
	}

	conf.table_index_mask = conf.table_buffer_size / TABLE_ELEMENT_SIZE - 1;

	INFO("location of files : %s\n", conf.location_of_files);
	INFO("indices buffer size: %u\n", conf.indices_buffer_size);
	INFO("table_buffer_size : %u\n", conf.table_buffer_size);
	INFO("table_index_mask : 0x%08X\n", conf.table_index_mask);

	return 0;
}

template<typename T>
static T* read_input_buffer(const char* const location, const char* const filename, uint32_t size)
{
	char path[2048];
	snprintf(path, sizeof(path) - 1, "%s/%s", location, filename);
	struct stat statbuf;
	if (stat(path, &statbuf) < 0)
	{
		ERR("stat(%s) failed\n", path);
		return nullptr;
	}
	if (statbuf.st_size < size)
	{
		ERR("size of file %s is lower then expected %u\n", path, size);
		return nullptr;
	}
	const int fd = open(path, O_RDONLY);
	if (fd < 0)
	{
		ERR("open(%s) failed\n", path);
		return nullptr;
	}
	T* input = (T*)malloc(size);
	if (!input)
	{
		ERR("malloc failed for %s\n", path);
		close(fd);
		return nullptr;
	}
	if (read(fd, input, size) != size)
	{
		close(fd);
		free(input);
		return nullptr;
	}
	close(fd);

	return input;
}

template<typename T>
static void free_input_buffer(T* buffer)
{
	if (buffer)
	{
		free((void*)buffer);
	}
}

static double get_clockdiff_ms(struct timespec *start, struct timespec *end)
{
	return ((double)end->tv_nsec/1000000.0 + (double)end->tv_sec*1000.0) -
			((double)start->tv_nsec/1000000.0 + (double)start->tv_sec*1000.0);
}

constexpr uint32_t THREADS_MAX = 256;

static void* thread_func(struct thread_data* thr_data)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(thr_data->id, &cpuset);
	pthread_t thread = pthread_self();
	pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);

	struct config*        conf = thr_data->conf;
	const uint32_t        count_of_input_indices = thr_data->common_data->count_of_input_indices;
	uint32_t* const       indices_arr = thr_data->common_data->indices;
	const uint16_t* const table = thr_data->common_data->table;
	uint16_t              value0 = TABLE_XOR_VAL;
	uint16_t              value1 = TABLE_XOR_VAL;
	uint16_t              value2 = TABLE_XOR_VAL;
	uint16_t              value3 = TABLE_XOR_VAL;
	const uint32_t        cycles = conf->cycle_count;
	const uint32_t        table_index_mask = conf->table_index_mask;
	struct timespec       start;
	struct timespec       end;
	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	for (uint32_t cycle = 0; cycle < cycles; ++cycle)
	{
		for (uint32_t index = 0; index < count_of_input_indices; index += 4)
		{
			__m128i indices = _mm_set_epi32(
					(indices_arr[index    ] ^ INDEX_XOR_VAL) + thr_data->id,
					(indices_arr[index + 1] ^ INDEX_XOR_VAL) + thr_data->id,
					(indices_arr[index + 2] ^ INDEX_XOR_VAL) + thr_data->id,
					(indices_arr[index + 3] ^ INDEX_XOR_VAL) + thr_data->id);

			value0 = (value0 ^ table[_mm_extract_epi32(indices, 0) & table_index_mask]) & TABLE_ADD_VAL;
			value1 = (value1 ^ table[_mm_extract_epi32(indices, 1) & table_index_mask]) & TABLE_ADD_VAL;
			value2 = (value2 ^ table[_mm_extract_epi32(indices, 2) & table_index_mask]) & TABLE_ADD_VAL;
			value3 = (value3 ^ table[_mm_extract_epi32(indices, 3) & table_index_mask]) & TABLE_ADD_VAL;

			indices_arr[index    ] = _mm_extract_epi32(indices, 0);
			indices_arr[index + 1] = _mm_extract_epi32(indices, 1);
			indices_arr[index + 2] = _mm_extract_epi32(indices, 2);
			indices_arr[index + 3] = _mm_extract_epi32(indices, 3);
		}
	}
	clock_gettime(CLOCK_MONOTONIC_RAW, &end);

	thr_data->table_accesses = cycles * count_of_input_indices;
	thr_data->clock_sum = get_clockdiff_ms(&start, &end);
	//printf("%u %u %u %u\n", value0, value1, value2, value3);
	thr_data->value = value0 ^ value1 ^ value2 ^ value3;

	return nullptr;
}

int main(int argc, char *argv[])
{
	const char* error_message = nullptr;
	auto handle_error_message = scope_exit([&]()
			{
				if (error_message != nullptr)
				{
					ERR("%s\n", error_message);
				}
			});

	struct config conf;
	if (parse_args(argc, argv, conf) < 0)
	{
		error_message = "failed to parse command line arguments";
		return -1;
	}

	uint32_t* const indices = read_input_buffer<uint32_t>(
			conf.location_of_files,
			FILE_WITH_INDICES,
			conf.indices_buffer_size);
	if (!indices)
	{
		error_message = "failed to read buffer with indices";
		return -1;
	}

	const uint16_t* const table = read_input_buffer<uint16_t>(
			conf.location_of_files,
			FILE_WITH_TABLE,
			conf.table_buffer_size);
	if (!table)
	{
		error_message = "failed to read buffer with table";
		free_input_buffer(indices);
		return -1;
	}

	const uint32_t            count_of_input_indices = conf.indices_buffer_size / sizeof(uint32_t);
	const uint32_t            count_of_table_elements = conf.table_buffer_size / TABLE_ELEMENT_SIZE;
	struct thread_common_data thr_common_data(indices, table, count_of_input_indices, count_of_table_elements);
	struct thread_data        thr_data[THREADS_MAX] = {};
	pthread_attr_t            thread_attr;
	pthread_attr_init(&thread_attr);
	pthread_t                 threads[THREADS_MAX] = {};
	uint32_t                  thread_count = 0;
	for (uint32_t thread_id = 0; thread_id < conf.thread_count; ++thread_id)
	{
		thr_data[thread_id].conf = &conf;
		thr_data[thread_id].common_data = &thr_common_data;
		thr_data[thread_id].id = thread_id;
		if (pthread_create(
				&threads[thread_id],
				&thread_attr,
				(void*(*)(void*))thread_func,
				(void*)&thr_data[thread_id]) != 0)
		{
			break;
		}
		thread_count++;
	}

	for (uint32_t thread_id = 0; thread_id < thread_count; ++thread_id)
	{
		pthread_join(threads[thread_id], nullptr);
	}

	if (thread_count < conf.thread_count)
	{
		// Not all threads were created, so the test is irrelevant.
		error_message = "test failed";
		free_input_buffer(table);
		free_input_buffer(indices);
		return -1;
	}

	uint64_t table_accesses = 0;
	uint16_t value = 0;
	double clock_sum = 0.0;
	double clock_sum_max = 0.0;
	double throughput_sum = 0.0;
	for (uint32_t thread_id = 0; thread_id < thread_count; ++thread_id)
	{
		table_accesses += thr_data[thread_id].table_accesses;
		clock_sum += thr_data[thread_id].clock_sum;
		clock_sum_max = std::max(clock_sum_max, thr_data[thread_id].clock_sum);
		value += thr_data[thread_id].value;
		throughput_sum += ((thr_data[thread_id].table_accesses / 1000.0) / thr_data[thread_id].clock_sum);
	}
	uint64_t table_accesses_avg = table_accesses / thread_count;
	double clock_sum_avg = clock_sum / (double)thread_count;

	INFO("table accesses: %zu\n", table_accesses);
	INFO("clockdiff: %.4f ms\n", clock_sum);
	const double data_read_written = table_accesses * sizeof(uint16_t);
	INFO("data_read_written: %.4f\n", data_read_written);
	INFO("throughput: %.4f MB/s\n", (data_read_written / 1000.0) / clock_sum);
	INFO("transactions: AVG per thread %.4f MT/s (a=%zu dt=%.4f), AVG all threads %.4f MT/s (a=%zu dt=%.4f), %.4f MT/s (a=%zu dt=%.4f) THR sum %.4f MT/s\n",
			(table_accesses_avg / 1000.0) / clock_sum_avg, table_accesses_avg, clock_sum_avg,
			(table_accesses / 1000.0) / clock_sum, table_accesses, clock_sum,
			(table_accesses / 1000.0) / clock_sum_max, table_accesses, clock_sum_max,
			throughput_sum);
	INFO("value: %u\n", value);

	free_input_buffer(table);
	free_input_buffer(indices);

	return value;
}
