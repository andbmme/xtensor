#ifndef XUTILS_HPP
#define XUTILS_HPP

#include <utility>
#include <tuple>
#include <type_traits>

namespace qs
{

    template <class F, class... T>
    void for_each(F&& f, std::tuple<T...>& t);

    template <class F, class... Args>
    void for_each_arg(F&&, Args&&...);

    template <class F, class R, class... T>
    R accumulate(F&& f, R init, const std::tuple<T...>& t);

    template <class F, class R, class... Args>
    R accumulate_arg(F&& f, R init, Args&&... args);

    template <class... T>
    struct or_;

    template <size_t I, class... Args>
    decltype(auto) argument(Args&&... args) noexcept;

    template<class R, class F, class... S>
    R apply(size_t index, F&& func, S... s);

    /***********************
     * for_each on tuple
     ***********************/

    namespace detail
    {
        template <size_t I, class F, class... T>
        inline typename std::enable_if<I == sizeof...(T), void>::type
        for_each_impl(F&& f, std::tuple<T...>& t)
        {
        }

        template <size_t I, class F, class... T>
        inline typename std::enable_if<I < sizeof...(T), void>::type
        for_each_impl(F&& f, std::tuple<T...>& t)
        {
            f(std::get<I>(t));
            for_each_impl<I + 1, F, T...>(std::forward<F>(f), t);
        }
    }

    template <class F, class... T>
    inline void for_each(F&& f, std::tuple<T...>& t)
    {
        detail::for_each_impl<0, F, T...>(std::forward<F>(f), t);
    }


    /*********************************
     * for_each_arg implementation
     *********************************/

    namespace detail
    {
        template <size_t I>
        struct invoker_base
        {
            template <class F, class T>
            invoker_base(F&& f, T&& t)
            {
                f(std::forward<T>(t));
            }
        };

        template <size_t... Ints>
        struct invoker : invoker_base<Ints>...
        {
            template <class F, class... Args>
            invoker(F&& f, Args&&... args)
                : invoker_base<Ints>(std::forward<F>(f), std::forward<Args>(args))...
            {
            }
        };

        template <class F, size_t... Ints, class... Args>
        inline void for_each_arg_impl(F&& f, std::index_sequence<Ints...>, Args&&... args)
        {
            invoker<Ints...> invoker(std::forward<F>(f), std::forward<Args>(args)...);
        }
    }

    template <class F, class... Args>
    inline void for_each_arg(F&& f, Args&&... args)
    {
        detail::for_each_arg_impl(std::forward<F>(f), std::make_index_sequence<sizeof...(Args)>(), std::forward<Args>(args)...);
    }


    /***********************************
     * accumulate_arg arguments
     ***********************************/

    namespace detail
    {
        template <size_t I, size_t N>
        struct accumulator : accumulator<I+1, N>
        {
            using base_type = accumulator<I+1, N>;

            template <class F, class R, class T, class... Args>
            inline R apply(F&& f, R r, T&& t, Args&&... args) const
            {
                R res = f(r, std::forward<T>(t));
                res = base_type::apply(std::forward<F>(f), res, std::forward<Args>(args)...);
                return res;
            }
        };

        template <size_t N>
        struct accumulator<N, N>
        {

            template <class F, class R, class T>
            inline R apply(F&& f, R r, T&& t) const
            {
                return f(r, std::forward<T>(t));
            }
        };
    }

    template <class F, class R, class... Args>
    inline R accumulate_arg(F&& f, R init, Args&&... args)
    {
        detail::accumulator<1, sizeof...(Args)> ac;
        return ac.apply(std::forward<F>(f), init, std::forward<Args>(args)...);
    }


    /*************************
     * accumulate tuple
     *************************/

    namespace detail
    {
        template <size_t I, class F, class R, class... T>
        inline std::enable_if_t<I == sizeof...(T), R>
        accumulate_impl(F&& f, R init, const std::tuple<T...>& t)
        {
            return init;
        }

        template <size_t I, class F, class R, class... T>
        inline std::enable_if_t<I < sizeof...(T), R>
        accumulate_impl(F&& f, R init, const std::tuple<T...>& t)
        {
            R res = f(init, std::get<I>(t));
            return accumulate_impl<I + 1, F, R, T...>(std::forward<F>(f), res, t);
        }
    }


    template <class F, class R, class... T>
    inline R accumulate(F&& f, R init, const std::tuple<T...>& t)
    {
        return detail::accumulate_impl<0, F, R, T...>(f, init, t);
    }


    /**********************
     * or_ metafunction
     **********************/

    template <class T>
    struct or_<T> : std::integral_constant<bool, T::value>
    {
    };

    template <class T, class... Ts>
    struct or_<T, Ts...>
        : std::integral_constant<bool, T::value || or_<Ts...>::value>
    {
    };

    /**********************
     * argument
     **********************/
 
    namespace detail
    {
        template <size_t I>
        struct getter
        {
            template <class Arg, class... Args>
            static inline decltype(auto) get(Arg&& arg, Args&&... args) noexcept
            {
                return getter<I - 1>::get(std::forward<Args>(args)...);
            }
        };

        template <>
        struct getter<0>
        {
            template <class Arg, class... Args>
            static inline Arg&& get(Arg&& arg, Args&&... args) noexcept
            {
                return std::forward<Arg>(arg);
            }
        };
    }

    template <size_t I, class... Args>
    inline decltype(auto) argument(Args&&... args) noexcept
    {
        static_assert(I < sizeof...(Args), "I should be lesser than sizeof...(Args)");
        return detail::getter<I>::get(std::forward<Args>(args)...);
    }
    
    /************************
     * apply implementation
     ************************/

    namespace detail
    {

        template<class R, class F, int I, class... S>
        R apply_one(F&& func, S&&... s)
        {
            return func(argument<I>(s...));
        }

        template<class R, class F, size_t... I, class... S>
        R apply(size_t index, F&& func, std::index_sequence<I...>, S&&... s)
        {
            using FT = R(F, S&&...);
            static constexpr FT* arr[] = { &apply_one<R, F, I, S...>... };
            return arr[index](std::forward<F>(func), std::forward<S>(s)...);
        }
    }

    template<class R, class F, class... S>
    R apply(size_t index, F&& func, S... s)
    {
        return detail::apply<R>(index, std::forward<F>(func), std::make_index_sequence<sizeof...(S)>(), std::forward<S>(s)...);
    }
}

#endif

