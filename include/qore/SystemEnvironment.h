/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  SystemEnvironment.h

  Qore Programming Language

  Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.

  Note that the Qore library is released under a choice of three open-source
  licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
  information.
*/

#ifndef _QORE_SYSTEMENVIRONMENT_H

#define _QORE_SYSTEMENVIRONMENT_H

#include <cstddef>
#include <string>

class QoreString;
class QoreStringNode;

//! class used to safely read and change the process environment from several threads
/** The C library's environment functions (\c getenv(), \c setenv(), \c unsetenv(), \c putenv()) and the \c environ
    array cannot be used safely from several threads at once while one of them changes the environment:
    \c setenv() may free the value it replaces (musl's does) and may reallocate the \c environ array (every C
    library), so a thread reading the environment at the same time can read freed memory, and two threads changing
    it at the same time can free the same memory twice.

    %Qore therefore serializes every access to the process environment that it makes with one process-wide
    read-write lock, on all platforms: reads hold it shared, so they run in parallel, and changes hold it
    exclusively.  This class reads and changes the environment under that lock; code that uses the environment in
    a module must use the same lock:
    - to read a variable: isSet(), get(), getAsStringNode() or QoreEnvironmentValue
    - to change a variable: set() or unset()
    - to read and change several variables atomically: AtomicEnvironmentSetter, which holds the lock exclusively for
      its lifetime
    - to read the environment with the C library directly (for example to iterate \c environ, to pass \c environ to
      \c posix_spawn() or \c execve(), or to call a third-party function that reads the environment): hold a
      QoreEnvironmentReadLock

    A thread may nest these calls: a thread that holds the lock, shared or exclusively, can call any of them again,
    except that a thread holding the lock only shared (with a QoreEnvironmentReadLock) cannot change the environment:
    set() and unset() then return -1 with \c errno \c EDEADLK, as do the AtomicEnvironmentSetter functions that
    change the environment, since the thread cannot wait for the exclusive lock while it holds the lock itself.

    A child process created with \c fork() starts with the lock released, or held as the forking thread held it
    (so that a lock object of the forking thread can be released in the child), whatever other threads held it.

    @note Only code that takes the lock is serialized with %Qore: native code that reads or changes the environment
    without it, for example a third-party library calling \c getenv() or \c setenv() internally, or a language
    runtime embedded in a module, is not covered.  Such code must not change the environment while other threads may
    use it, and reads it at its own risk while %Qore code may change it.

    There is only one of these objects; the constructor and destructor are not exported in the public interface of
    the library and therefore can only be instantiated internally anyway.

    @see AtomicEnvironmentSetter
    @see QoreEnvironmentReadLock
    @see QoreEnvironmentValue
 */
class SystemEnvironment {
    friend class AtomicEnvironmentSetter;

private:
    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL SystemEnvironment(const SystemEnvironment&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL SystemEnvironment& operator=(const SystemEnvironment&);

protected:
    //! sets the given environment variable to the value passed, respecting the overwrite flag
    /** the caller must hold the environment lock exclusively
        @param name the name of the environment variable to set
        @param value the value of the environment variable
        @param overwrite the overwrite flag
     */
    DLLLOCAL static int set_intern(const char* name, const char* value, bool overwrite = 1);

    //! returns the value of the environment variable as a new QoreString object, 0 if not present, caller owns the pointer returned
    /** the caller must hold the environment lock
        @param name the name of the environment variable
        @return a QoreString pointer (or 0 if the variable does not exist), caller owns the pointer returned
     */
    DLLLOCAL static QoreString* get_intern(const char* name);

    //! returns the value of the environment variable as a new QoreStringNode object, 0 if not present, caller owns the reference count of the pointer returned
    /** the caller must hold the environment lock
        @param name the name of the environment variable
        @return a QoreStringNode pointer (or 0 if the variable does not exist), caller owns the reference count of the pointer returned
     */
    DLLLOCAL static QoreStringNode* get_as_string_node_intern(const char* name);

    //! appends the value of the given environment variable to a QoreString, returns 0 for OK, -1 for not found
    /** the caller must hold the environment lock
        @param name the name of the environment variable
        @param str a reference to a QoreString object where the value will be concatenated if the environment variable exists
        @return 0 for OK (environment variable found and value concatenated to string), or -1 for not found
     */
    DLLLOCAL static int get_intern(const char* name, QoreString& str);

    //! unsets the given environment variable
    /** the caller must hold the environment lock exclusively
        @param name the name of the environment variable to unset
        @return 0 for OK, non-0 for error
     */
    DLLLOCAL static int unset_intern(const char* name);

    //! returns true if the environment variable exists and has a value, false if not
    /** the caller must hold the environment lock
     */
    DLLLOCAL static bool value_exists_intern(const char* name);

public:
    DLLLOCAL SystemEnvironment();
    DLLLOCAL ~SystemEnvironment();

    //! sets the given environment variable to the value passed, respecting the overwrite flag
    /**
        @param name the name of the environment variable to set
        @param value the value of the environment variable
        @param overwrite the overwrite flag

        @return 0 for OK, -1 for error (with \c errno set); \c errno is \c EDEADLK if the current thread holds the
        environment lock shared only (a QoreEnvironmentReadLock)
     */
    DLLEXPORT static int set(const char* name, const char* value, bool overwrite = 1);

    //! returns the value of the environment variable as a new QoreString object, 0 if not present, caller owns the pointer returned
    /**
        @param name the name of the environment variable
        @return a QoreString pointer (or 0 if the variable does not exist), caller owns the pointer returned
     */
    DLLEXPORT static QoreString* get(const char* name);

    //! returns the value of the environment variable as a new QoreStringNode object, 0 if not present, caller owns the reference count of the pointer returned
    /**
        @param name the name of the environment variable
        @return a QoreStringNode pointer (or 0 if the variable does not exist), caller owns the reference count of the pointer returned
     */
    DLLEXPORT static QoreStringNode* getAsStringNode(const char* name);

    //! appends the value of the given environment variable to a QoreString, returns 0 for OK, -1 for not found
    /**
        @param name the name of the environment variable
        @param str a reference to a QoreString object where the value will be concatenated if the environment variable exists
        @return 0 for OK (environment variable found and value concatenated to string), or -1 for not found
     */
    DLLEXPORT static int get(const char* name, QoreString& str);

    //! unsets the given environment variable
    /**
        @param name the name of the environment variable to unset

        @return 0 for OK, -1 for error (with \c errno set); \c errno is \c EDEADLK if the current thread holds the
        environment lock shared only (a QoreEnvironmentReadLock)
     */
    DLLEXPORT static int unset(const char* name);

    //! returns true if the environment variable exists and has a value, false if not
    /**
        @param name the name of the environment variable to check
        @return true if the environment variable exists and has a value, false if not
     */
    DLLEXPORT static bool valueExists(const char* name); // true if exists and is not empty

    //! returns true if the environment variable is set, even if its value is empty
    /** The variable is looked up under the environment lock held shared; no memory is allocated.

        @param name the name of the environment variable to check

        @return true if the environment variable is set, false if not

        @since %Qore 3.0
     */
    DLLEXPORT static bool isSet(const char* name);
};

DLLEXPORT extern SystemEnvironment SysEnv;

//! holds the process environment lock exclusively for its lifetime to read and change the environment atomically; can only be used on the stack
/** While the object exists, no other thread can read or change the environment through %Qore's API, so several
    variables can be read and changed atomically.  The calling thread can also call SystemEnvironment functions, and
    it can change the environment with the C library directly (\c setenv(), \c unsetenv()).

    Keep the object only as long as necessary: every other thread that reads or changes the environment waits until
    it is destroyed.  Created by a thread that holds a QoreEnvironmentReadLock, the object holds the lock only
    shared (the thread cannot wait for the exclusive lock while it holds the lock itself): set() and unset() then
    return -1 with \c errno \c EDEADLK.

    @par Example:
    @code{.cpp}
// make two variables change together
{
    AtomicEnvironmentSetter aes;
    aes.set("APP_HOST", host);
    aes.set("APP_PORT", port);
}
    @endcode

    @see SystemEnvironment
    @see QoreEnvironmentReadLock
 */
class AtomicEnvironmentSetter {
private:
    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL AtomicEnvironmentSetter(const AtomicEnvironmentSetter&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL AtomicEnvironmentSetter& operator=(const AtomicEnvironmentSetter&);

    //! this function is not implemented; it is here as a private function in order to prohibit it from being used
    DLLLOCAL void* operator new(size_t); // not implemented, make sure it is not new'ed

public:
    //! acquires the environment lock exclusively
    DLLEXPORT AtomicEnvironmentSetter();

    //! releases the environment lock
    DLLEXPORT ~AtomicEnvironmentSetter();

    //! sets the given environment variable to the value passed, respecting the overwrite flag
    /**
        @param name the name of the environment variable to set
        @param value the value of the environment variable
        @param overwrite the overwrite flag

        @return 0 for OK, -1 for error (with \c errno set); \c errno is \c EDEADLK if the object holds the
        environment lock only shared, because the thread that created it holds a QoreEnvironmentReadLock
     */
    DLLEXPORT int set(const char* name, const char* value, bool overwrite = 1);

    //! returns the value of the environment variable as a new QoreString object, 0 if not present, caller owns the pointer returned
    /**
        @param name the name of the environment variable
        @return a QoreString pointer (or 0 if the variable does not exist), caller owns the pointer returned
     */
    DLLEXPORT QoreString* get(const char* name);

    //! returns the value of the environment variable as a new QoreStringNode object, 0 if not present, caller owns the reference count of the pointer returned
    /**
        @param name the name of the environment variable
        @return a QoreStringNode pointer (or 0 if the variable does not exist), caller owns the reference count of the pointer returned
     */
    DLLEXPORT QoreStringNode* getAsStringNode(const char* name);

    //! appends the value of the given environment variable to a QoreString, returns 0 for OK, -1 for not found
    /**
        @param name the name of the environment variable
        @param str a reference to a QoreString object where the value will be concatenated if the environment variable exists
        @return 0 for OK (environment variable found and value concatenated to string), or -1 for not found
     */
    DLLEXPORT int get(const char* name, QoreString& str);

    //! unsets the given environment variable
    /**
        @param name the name of the environment variable to unset

        @return 0 for OK, -1 for error (with \c errno set); \c errno is \c EDEADLK if the object holds the
        environment lock only shared, because the thread that created it holds a QoreEnvironmentReadLock
     */
    DLLEXPORT int unset(const char* name);

    //! returns true if the environment variable exists and has a value, false if not
    /**
        @param name the name of the environment variable to check
        @return true if the environment variable exists and has a value, false if not
     */
    DLLEXPORT bool valueExists(const char* name); // true if exists and is not empty
};

//! holds the process environment lock shared (for reading) for its lifetime; can only be used on the stack
/** While the object exists, the environment cannot be changed through %Qore's API, so the \c environ array and the
    strings that \c getenv() returns stay valid and unchanged, while other threads can still read the environment.
    Use it to read the environment with the C library directly, for example to iterate \c environ, to pass
    \c environ to \c posix_spawn() or \c execve(), or to call a third-party function that reads the environment.

    Rules:
    - do not keep the object longer than necessary: changes to the environment wait until it is destroyed
    - do not change the environment while holding it: SystemEnvironment::set(), SystemEnvironment::unset() and the
      functions of an AtomicEnvironmentSetter that change the environment return -1 with \c errno \c EDEADLK;
      reading it through SystemEnvironment, QoreEnvironmentValue or another QoreEnvironmentReadLock is allowed
    - pointers into the environment are valid only while the object exists; copy what is needed

    @par Example:
    @code{.cpp}
// pass a consistent copy of the environment to a child process
{
    QoreEnvironmentReadLock env_lock;
    rc = posix_spawn(&pid, path, &actions, &attr, argv, environ);
}
    @endcode

    @see SystemEnvironment
    @see AtomicEnvironmentSetter

    @since %Qore 3.0
 */
class QoreEnvironmentReadLock {
public:
    //! acquires the environment lock shared
    DLLEXPORT QoreEnvironmentReadLock();

    //! releases the environment lock
    DLLEXPORT ~QoreEnvironmentReadLock();

    QoreEnvironmentReadLock(const QoreEnvironmentReadLock&) = delete;
    QoreEnvironmentReadLock& operator=(const QoreEnvironmentReadLock&) = delete;
    void* operator new(size_t) = delete;
};

//! a copy of the value of an environment variable, read under the process environment lock
/** The value is copied when the object is created, so it stays valid however the environment changes afterwards;
    use it instead of \c getenv(), whose result can be freed by a concurrent change to the environment.

    @par Example:
    @code{.cpp}
QoreEnvironmentValue threads("APP_THREADS");
int n = threads ? atoi(threads.c_str()) : 4;
    @endcode

    @see SystemEnvironment

    @since %Qore 3.0
 */
class QoreEnvironmentValue {
public:
    //! reads the given environment variable and keeps a copy of its value
    /** @param name the name of the environment variable
     */
    DLLEXPORT explicit QoreEnvironmentValue(const char* name);

    //! returns true if the variable was set when this object was created
    explicit operator bool() const {
        return is_set;
    }

    //! returns true if the variable was set when this object was created
    bool isSet() const {
        return is_set;
    }

    //! returns the value, or nullptr if the variable was not set; the pointer is valid while the object exists
    const char* c_str() const {
        return is_set ? value.c_str() : nullptr;
    }

    //! returns the value; empty if the variable was not set
    const std::string& str() const {
        return value;
    }

private:
    std::string value;
    bool is_set = false;
};

#endif
