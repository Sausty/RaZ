#pragma once

#ifndef RAZ_THREADING_HPP
#define RAZ_THREADING_HPP

// std::thread is not available on MinGW with Win32 threads
#if !defined(__MINGW32__) || defined(_GLIBCXX_HAS_GTHREADS)
#define RAZ_THREADS_AVAILABLE
#endif

#ifdef RAZ_THREADS_AVAILABLE

#include <future>

namespace Raz {

namespace Threading {

/// Calls a function asynchronously, to be executed without blocking the calling thread.
/// \tparam T Return type of the given function.
/// \tparam Args Types of the arguments to be forwarded to the given function.
/// \param action Action to be performed asynchronously.
/// \param args Arguments to be forwarded to the given function.
/// \return A std::future holding the future result of the process.
template <typename Func, typename... Args, typename ResultType = std::result_of_t<Func&&(Args&&...)>>
std::future<ResultType> launchAsync(Func action, Args&&... args);

} // namespace Threading

} // namespace Raz

#include "Threading.inl"

#else
#pragma message("Warning: Threads are not available with your compiler; check that you're using POSIX threads and not Win32 ones.")
#endif // RAZ_THREADS_AVAILABLE

#endif // RAZ_THREADING_HPP
