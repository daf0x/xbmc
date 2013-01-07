#pragma once
/*
 *      Copyright (C) 2005-2012 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <sys/types.h>
#include <boost/utility.hpp>
#include "threads/Condition.h"

/**
 * Very simple process watcher. The goal is to be able to safely watch a PID
 * which may be constructed in a different thread. In order to allow the
 * spawning to occur in a different thread (so as to cause minimal slowdown
 * of the originating thread) PIDWatcher allows the assignment of the PID to
 * be postponed, any subsequent function calls to PIDWatcher will only wait
 * for a PID to be assigned when necessary. The default state (before
 * assignment of a PID) is to assume that the child process was started
 * successfully.
 * Thus to observers of PIDWatcher a process is always either running or
 * terminated.
 */
class PIDWatcher : public XbmcThreads::NonCopyable {
  private:
    CCriticalSection               pid_mutex;
    XbmcThreads::ConditionVariable pid_assigned_condition;

    // Whether PIDWatcher should wait for the child to die upon destruction.
    bool                           wait_exit_;

    // The following will be assigned only once, and are not modified after a
    // call to wait(). Hence any code following a call to wait can safely read
    // these variables without the need to aquire a lock.
    pid_t                         pid_;        // the pid to be watched.
    int                           status_;     // return code of waitpid()
    bool                           has_exited_; // true iff pid has exited

    /**
     * Makes sure that a valid PID is assigned to the watcher before
     * proceeding.
     */
    void ensure_pid(CSingleLock& lock);

    /**
     * Wait for child to exit. Does not wait for assignment of PID.
     */
    PIDWatcher& wait_(CSingleLock& lock);

  public:
    /**
     * Construct an empty watcher object. Use set_pid() to set a pid.
     */
    PIDWatcher();

    /**
     * Construct a watcher object for (child) process with PID pid.
     */
    PIDWatcher(pid_t pid);

    /**
     * Destructor. PIDWatcher will wait for any child to exit, unless reset()
     * was called prior.
     */
    ~PIDWatcher();

    /**
     * Start watching process with PID pid.
     * NOTE: PIDWatcher can only be assigned a PID once, although at present it
     *       is allowed to assign the same PID to the watcher more then once,
     *       without this being considered an error (it is treated as a no-op).
     * XXX: Should we allow to assign a new PID? How do other users (threads)
     *      know that we are now watching a different PID? Perhaps allow
     *      re-assignment only when there are no waiters?
     */
    PIDWatcher& set_pid(pid_t pid);

    /**
     * Returns the currently assigned PID.
     */
    pid_t get_pid();

    /**
     * Wait for child to exit.
     */
    PIDWatcher& wait();

    /**
     * Clears this PID watcher object (so it will not wait for the child to
     * exit). After this call the child process will be considered to have
     * exited, e.g. any future call to running() will return false.
     */
    PIDWatcher& reset();

    /**
     * Signals the child process that it should terminate as soon as possible.
     * To immediately terminate the child process use terminate_now(). To wait
     * for the process to really die use something like
     * watcher.terminate().wait();
     */
    PIDWatcher& terminate();

    /**
     * Terminate the child process immediately.
     *
     * This will first signal the child to exit, similarly to calling just
     * terminate(). If however the child process fails to exit in a timely
     * fashion, it will be terminated forcibly.
     * NOTE: Unlike terminate(), this call does not return until the child
     *       process has been terminated.
     * XXX: To be implemented
     */
    PIDWatcher& terminate_now(long timeout_millis = 200);

    /**
     * Returns true iff child is executing.
     */
    bool running();

    /**
     * Indicates whether or not PIDWatcher should wait for the PID to terminate
     * upon its destruction.
     */
    bool getWaitExit();

    /**
     * Sets whether or not PIDWatcher should wait for the PID to terminate upon
     * its destruction.
     */
    PIDWatcher& setWaitExit(bool b);

    /**
     * Returns true iff the watched pid exited gracefully (e.g. it did not
     * crash).
     * NOTE: Implies wait().
     */
    bool exitedProperly();

    /**
     * Returns the child's exit status code. This is valid iff
     * exitedProperly().
     * NOTE: Implies wait().
     */
    int getExitStatus();

    /**
     * Returns true iff getExitStatus indicated no error.
     * NOTE: Implies wait().
     */
    bool success();

    /**
     * Returns true iff pid is currently valid (i.e. a pid has been assigned).
     */
    bool has_pid();

    /**
     * Equivalent to has_pid().
     * XXX: Should we keep this? Does it serve a useful purpose?
     */
    operator const bool();
};
