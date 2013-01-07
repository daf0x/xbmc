#include "PIDWatcher.h"
#include "utils/log.h"
#include <sys/wait.h>
#include <boost/lexical_cast.hpp>

using namespace std;


PIDWatcher::PIDWatcher()
: wait_exit_(true)
, pid_(0)
, status_(0)
, has_exited_(false)
{}

PIDWatcher::PIDWatcher(pid_t pid)
: wait_exit_(true)
, pid_(pid)
, status_(0)
, has_exited_(false)
{}

PIDWatcher::~PIDWatcher() {
  CSingleLock lock(pid_mutex);
  if(has_pid() && wait_exit_)
    wait();
}

void PIDWatcher::ensure_pid(CSingleLock& lock) {
  if(!has_pid())
    pid_assigned_condition.wait(lock);
}

bool PIDWatcher::running() {
  CSingleLock lock(pid_mutex);
  // http://stackoverflow.com/questions/9152979/check-if-process-exists-given-its-pid
  if(!has_exited_ && has_pid()) {
     has_exited_ = kill(pid_, 0) != 0;
  }

  // Be optimistic and assume that a PID will be assigned eventually
  return !has_exited_;
}

PIDWatcher& PIDWatcher::set_pid(pid_t pid) {
  if(pid < 1)
    //  NOTE: This is not really an invalid PID as far as the PID handling
    //        functions are concerned, but PIDWatcher is intended to watch a
    //        *single* PID, not a PID group.
    throw runtime_error("Attempt to assign an invalid PID: " + boost::lexical_cast<string>(pid));

  CSingleLock lock(pid_mutex);
  if(has_pid() && pid != pid_)
    // NOTE: Allowing to assign a new (different) PID leads to strange
    //       semantics for users of PIDWatcher. How should they react when
    //       they are suddenly watching a different PID then they thought?
    throw runtime_error("PIDWatcher is already watching a PID!");

  pid_     = pid;
  pid_assigned_condition.notifyAll();

  return *this;
}

pid_t PIDWatcher::get_pid() {
  CSingleLock lock(pid_mutex);
  ensure_pid(lock);
  return pid_;
}

PIDWatcher& PIDWatcher::wait_(CSingleLock& lock) {
  /**
   * NOTE:
   *   this is called from set_pid(), in which case there is no need to
   *   ensure_pid(), as we are now being assigned one.
   *   Also the lock is there to ensure that we have a lock on pid_, as we try
   *   to avoid locking our mutex recursively as much as possible (it's still
   *   locked again in has_pid() though).
   */
  if(has_pid() && !has_exited_)
    waitpid(pid_, &status_, 0);
  has_exited_ = true;

  return *this;
}

PIDWatcher& PIDWatcher::wait() {
  CSingleLock lock(pid_mutex);
  ensure_pid(lock);
  return wait_(lock);
}

PIDWatcher& PIDWatcher::reset() {
  CSingleLock lock(pid_mutex);
  pid_        = 0;
  status_     = 0;
  has_exited_ = true;

  return *this;
}


PIDWatcher& PIDWatcher::terminate() {
  CSingleLock lock(pid_mutex);
  if(has_pid() && !has_exited_) {
    if(kill(pid_, SIGTERM))
      CLog::Log(LOGERROR, "%s: failed to terminate child: %s", __FUNCTION__, strerror(errno));
  }

  return *this;
}

bool PIDWatcher::getWaitExit() {
  CSingleLock lock(pid_mutex);
  return wait_exit_;
}

PIDWatcher& PIDWatcher::setWaitExit(bool b) {
  CSingleLock lock(pid_mutex);
  wait_exit_ = b;

  return *this;
}

bool PIDWatcher::exitedProperly() {
  wait();
  return WIFEXITED(status_);
}

int PIDWatcher::getExitStatus() {
  wait();
  return WEXITSTATUS(status_);
}

bool PIDWatcher::success() {
  wait();
  return WIFEXITED(status_) && WEXITSTATUS(status_) == 0;
}

bool PIDWatcher::has_pid() {
  CSingleLock lock(pid_mutex);
  return pid_ > 0;
}

/*
PIDWatcher::operator const bool() {
  CSingleLock lock(pid_mutex);
  return has_pid();
}
*/
