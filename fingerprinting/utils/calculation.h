#ifndef CALCULATION_H
#define CALCULATION_H

namespace calculate
{
    template <typename Iterable>
    Iterable &square(Iterable &input)
    {
        for (auto &i : input)
        {
            i *= i;
        }
        return input;
    }

    template <typename Iterable>
    Iterable add(const Iterable &lhs, const Iterable &rhs)
    {
        Iterable result;
        result.resize(lhs.size());
        for (std::size_t i = 0; i < lhs.size(); ++i)
        {
            result[i] = lhs[i] + rhs[i];
        }
        return result;
    }

    template <typename Iterable>
    Iterable devide(const Iterable &lhs, const int rhs)
    {
        Iterable result;
        result.resize(lhs.size());
        for (std::size_t i = 0; i < lhs.size(); ++i)
        {
            result[i] = lhs[i] / rhs;
        }
        return result;
    }

    template <typename Iterable, typename T, int N>
    Iterable &multiply(Iterable &lhs, const T (&arr)[N])
    {
        for (std::size_t i = 0; i < lhs.size(); ++i)
        {
            lhs[i] *= arr[i];
        }
        return lhs;
    }

    template <typename Iterable, typename T>
    Iterable &max(Iterable &lhs, const T &rhs)
    {
        for (auto &i : lhs)
        {
            i = i > rhs ? i : rhs;
        }
        return lhs;
    }
} // namespace calculate

#endif // CALCULATION_H