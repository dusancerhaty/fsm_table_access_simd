#ifndef _SCOPE_GUARD_H_
#define _SCOPE_GUARD_H_

#include <utility>

/** Scope guard as proposed by Andrei Alexandrescu and used in the D language
 * and Boost.
 *
 * The scope guard object is constructed using `scope_exit()` with a function
 * parameter - that function will run when the current scope exit (called by
 * the scope guard's destructor). 
 *
 * Allows to specify deallocation of resources where they are allocated. This
 * makes functions with multiple return points more maintainable, since
 * we don't need to deallocate the resource at each point.
 *
 * The scope guard can be disabled using the `dismiss()` method if we no longer
 * want to execute it (useful if we only want to deallocate resources in case
 * of failure, for example if constructing a complex object).
 *
 * Design decisions:
 * - There is no `scope(failure)`/`scope(success)`, because we never use
 *   exceptions. `scope(failure)` is replaced by calling the `dismiss()`
 *   function in case of success. `scope(success)` was not very useful in
 *   general.
 * - Overhead is kept at minimum (no dynamic allocations, no `std::function()`.
 * - No macro magic is used, at least at the moment. Macros could allow better
 *   syntax, but at cost of hard to debug implementation / errors caused by
 *   incorrect use of such macros. See the following for good
 *   syntax/implementation with macros :
 *   https://cppsecrets.blogspot.com/2013/11/ds-scope-statement-in-c.html
 *   (it could be modified to support dismiss() by making visible guard object
 *   with some minimal changes).
 *
 * Example 1:
 * ~~~
 * {
 *     auto file = fopen( "path", "w" );
 *     if( !file )
 *     {
 *         return false;
 *     }
 *     auto guard = scope_exit( [&]{ fclose( file ); } );
 *
 *     // now do stuff with file
 *     ...
 *
 *
 *     // file is closed when we run out of scope
 *     return true;
 * }
 * ~~~
 *
 * Example 2:
 *
 * ~~~
 * {
 *     this->my_array = new uint8_t[256];
 *     auto guard = scope_exit( [&]{ delete[] this->my_array; } );
 *
 *     ...
 *
 *     if( error )
 *     {
 *         // my_array gets deleted
 *         return false;
 *     }
 *
 *     // success, keep my_array
 *     guard.dismiss();
 *     return true;
 * }
 * ~~~
 *
 * Code based on https://gist.github.com/mrts/5890888, which is based on
 * Andrei Alexandrescu's talks.
 */
template <class Function>
class scope_guard
{
public:
	/// Construct a scope guard that will call specified function at destruction.
	scope_guard(Function f)
		: guard_function( std::move(f) )
		, active( true )
	{
	}

	/// Destructor. Calls the function if active.
	~scope_guard()
	{
		if (active)
		{
			guard_function();
		}
	}

	/// Move constructor. Kept from source implementation, not sure how useful.
	scope_guard( scope_guard&& rhs )
		: guard_function( std::move(rhs.guard_function) )
		, active( rhs.active )
	{
		rhs.dismiss();
	}

	/// Dismiss the scope guard to prevent it from executing at destruction.
	void dismiss()
	{
		active = false;
	}

	/// Trigger the guard immediately, before exiting the scope.
	void trigger_early()
	{
		if (active)
		{
			guard_function();
			active = false;
		}
	}

private:
	// Function to call in destructor.
	Function guard_function;
	// Should we call guard_function from destructor?
	bool active = true;

	// No copying and no accidental use.
	scope_guard()                                = delete;
	scope_guard( const scope_guard& )            = delete;
	scope_guard& operator=( const scope_guard& ) = delete;
};

/** Function to instantiate a scope_guard w/o the user-unfriendly template param.
 *
 * @param f Function to run when the scope ends.
 */
template <class Function>
scope_guard<Function> scope_exit( Function f )
{
	return scope_guard<Function>( std::move(f) );
}

/// Template recursion end point for dismiss_guards().
inline void dismiss_guards()
{
}

/** Shortcut function to dismiss multiple scope guards.
 *
 * Unrolls at compile time.
 *
 * Example:
 *
 * ~~~
 * dismiss_guards( guard_1, guard_2, guard_3 );
 * ~~~
 */
template< typename Function, typename ... Args >
void dismiss_guards( scope_guard<Function>& first, Args&& ... args )
{
	first.dismiss();
	dismiss_guards( std::forward<Args>(args) ... );
}

#endif /* end of include guard: _SCOPE_GUARD_H_ */

