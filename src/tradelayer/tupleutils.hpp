#ifndef TUPLE_UTILS_HPP
#define TUPLE_UTILS_HPP

#include <type_traits>

#define BOOST_LEXICAL_CAST_ASSUME_C_LOCALE
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lexical_cast/try_lexical_convert.hpp>
#include <boost/throw_exception.hpp>

inline std::vector<std::string> split_string(const std::string& s, const std::string& separator = ";,")
{
    std::vector<std::string> ss;
    boost::split(ss, s, boost::is_any_of(separator), boost::algorithm::token_compress_on);
    return ss;
}

// C++11: compile-time helper to generate indices
template <size_t... Ns>
struct indices
{
    typedef indices<Ns..., sizeof...(Ns)> next;
};

template <size_t N>
struct make_indices
{
    typedef typename make_indices<N - 1>::type::next type;
};

template <>
struct make_indices<0>
{
    typedef indices<> type;
};
// C++11: compile-time helper to generate indices

template <typename... Args>
struct TupleTraits
{
    static constexpr auto Size = sizeof...(Args);
    using Tuple = std::tuple<Args...>;
    template <std::size_t N>
    using Nth = typename std::tuple_element<N, Tuple>::type;
    using First = Nth<0>;
    using Last = Nth<Size - 1>;
};

template <typename T>
struct NumericTraits
{
    constexpr static bool IsChar = std::is_same<T, bool>::value || std::is_same<T, char>::value || std::is_same<T, uint8_t>::value;

    struct Target
    {
        using Type = typename std::conditional<IsChar, int, T>::type;
    };
};

template <typename T>
struct LexicalTraits
{
    struct Target
    {
        using Type = T;
    };
};

// throw_bad_cast if true
struct BadCastPolicy
{
    constexpr static bool ThrowOnFailure = true;
};

struct NoThrowPolicy
{
    constexpr static bool ThrowOnFailure = false;
};

template 
<
    typename Policy = BadCastPolicy,
    template <typename> class Traits = NumericTraits
>
class Convertor
{
    static inline std::string trim_plus(const std::string& s)
    {
        size_t plus = s.find_first_not_of("+");
        return (plus == std::string::npos) ? s : s.substr(plus);
    }

    template <class T, typename std::enable_if<std::is_constructible<T, std::string>::value>::type* = nullptr>
    static inline T Cast(const std::string& s)
    {
        return s;
    }

    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
    static inline T Cast(const std::string& s)
    {
        static_assert(std::is_arithmetic<T>::value);

        using N = typename Traits<T>::Target::Type;
        N n{};
        if (!boost::conversion::detail::try_lexical_convert(s, n)) {
            if (Policy::ThrowOnFailure) BOOST_THROW_EXCEPTION(boost::bad_lexical_cast(typeid(s), typeid(n)));
        }
        return (T)n;
    }

    template <typename T>
    static inline T Get(const std::vector<std::string>& ss, size_t i)
    {
        // if index out of bounds just return T()
        if (ss.size() - 1 < i)
            return T();
        auto s = ss.at(i);
        boost::algorithm::trim(s);
        return s.empty() ? T() : Cast<T>(s);
    }

    template <typename... Args, std::size_t... I>
    static inline std::tuple<Args...> MakeTuple(const std::vector<std::string>& ss, indices<I...>)
    {
        return std::make_tuple(Get<Args>(ss, I)...);
    }

public:
    //! Returns a tuple<...> parsed
    template <typename... Args>
    static inline std::tuple<Args...> Parse(const std::string& s, const std::string& separator = ";,")
    {
        using R = TupleTraits<Args...>;
        // using T = typename R::Tuple;
        using I = typename make_indices<R::Size>::type;
        return MakeTuple<Args...>(split_string(s, separator), I());
    }

    //! Returns std::pair<count,tuple<...>>: count of elements parsed and used to generate a tuple
    template <typename... Args>
    static inline std::pair<size_t, std::tuple<Args...>> ParseEx(const std::string& s, const std::string& separator = ";,")
    {
        using R = TupleTraits<Args...>;
        using I = typename make_indices<R::Size>::type;
        const auto& v = split_string(s, separator);
        size_t n = v.size() > R::Size ? R::Size : v.size();
        return std::make_pair(n, MakeTuple<Args...>(v, I()));
    }
};

// Numeric Convertor: NC::Parse<bool,int64_t>("1;65536");
using NC = Convertor<BadCastPolicy, NumericTraits>;
using LC = Convertor<BadCastPolicy, LexicalTraits>;

#endif // TUPLE_UTILS_HPP