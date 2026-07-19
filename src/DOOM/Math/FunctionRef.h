#pragma once

#include <type_traits>
#include <utility>

namespace Doom
{
// A non-owning view of any callable with a given signature.
//
// This exists so a hot-path function can take a lambda WITH CAPTURES without
// paying for it. A raw function pointer cannot carry context at all - which is
// why so much of vanilla DOOM passes context through globals instead - and a
// std::function would own, and may allocate. A FunctionRef is two pointers, is
// trivially copyable, and calls through one indirection.
//
// It does NOT extend the lifetime of what it points at, so it is only safe as a
// parameter, which is the only way the engine uses it.
template <class Signature>
class FunctionRef;

template <class Result, class... Args>
class FunctionRef<Result(Args...)>
{
public:
    // A plain function. Stored as its own pointer and cast straight back, which
    // is a round trip and so well defined - the same technique Sim/Info.cpp's
    // states[] table uses for the action functions.
    FunctionRef(Result (*function)(Args...))
        : object(reinterpret_cast<void*>(function))
        , invoke([](void* obj, Args... args) -> Result {
              return reinterpret_cast<Result (*)(Args...)>(obj)(
                  std::forward<Args>(args)...);
          })
    {
    }

    // Anything else - in practice a lambda, whose captures ride along in the
    // object it points at.
    template <class Callable,
              class = std::enable_if_t<
                  !std::is_same_v<std::decay_t<Callable>, FunctionRef>
                  && !std::is_convertible_v<Callable, Result (*)(Args...)>
                  && std::is_invocable_r_v<Result, Callable&, Args...>>>
    FunctionRef(Callable&& callable)
        : object(const_cast<void*>(static_cast<const void*>(&callable)))
        , invoke([](void* obj, Args... args) -> Result
                 { return (*static_cast<std::decay_t<Callable>*>(obj))(
                       std::forward<Args>(args)...); })
    {
    }

    Result operator()(Args... args) const
    {
        return invoke(object, std::forward<Args>(args)...);
    }

private:
    void* object = nullptr;
    Result (*invoke)(void*, Args...) = nullptr;
};
} // namespace Doom
