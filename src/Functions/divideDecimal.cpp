#include <Functions/FunctionsDecimalArithmetics.h>
#include <Functions/FunctionFactory.h>

namespace DB
{

namespace ErrorCodes
{
    extern const int DECIMAL_OVERFLOW;
    extern const int ILLEGAL_DIVISION;
}

namespace
{

struct DivideDecimalsImpl
{
    static constexpr auto name = "divideDecimal";

    template <typename FirstType, typename SecondType>
    static inline Decimal256
    execute(FirstType a, SecondType b, UInt16 scale_a, UInt16 scale_b, UInt16 result_scale)
    {
        if (b.value == 0)
            throw DB::Exception("Division by zero", ErrorCodes::ILLEGAL_DIVISION);
        if (a.value == 0)
            return Decimal256(0);

        Int256 sign_a = a.value < 0 ? -1 : 1;
        Int256 sign_b = b.value < 0 ? -1 : 1;

        std::vector<UInt8> a_digits = DecimalOpHelpers::toDigits(a.value * sign_a);

        while (scale_a < scale_b + result_scale)
        {
            a_digits.push_back(0);
            ++scale_a;
        }

        while (scale_a > scale_b + result_scale && !a_digits.empty())
        {
            a_digits.pop_back();
            --scale_a;
        }

        if (a_digits.empty())
            return Decimal256(0);

        std::vector<UInt8> divided = DecimalOpHelpers::divide(a_digits, b.value * sign_b);

        if (divided.size() > DecimalUtils::max_precision<Decimal256>)
            throw DB::Exception("Numeric overflow: result bigger that Decimal256", ErrorCodes::DECIMAL_OVERFLOW);
        return Decimal256(sign_a * sign_b * DecimalOpHelpers::fromDigits(divided));
    }
};

}

REGISTER_FUNCTION(DivideDecimals)
{
    factory.registerFunction<FunctionsDecimalArithmetics<DivideDecimalsImpl>>(Documentation(
        R"(
Performs division on two decimals. Result value will be of type [Decimal256](../../sql-reference/data-types/decimal.md).
Result scale can be explicitly specified by `result_scale` argument (const Integer in range `[0, 76]`). If not specified, the result scale is the max scale of given arguments.

:::note
These function work significantly slower than usual `divide`.
In case you don't really need controlled precision and/or need fast computation, consider using [divide](#divide).
:::

**Syntax**

```sql
divideDecimal(a, b[, result_scale])
```

**Arguments**

-   `a` — First value: [Decimal](../../sql-reference/data-types/decimal.md).
-   `b` — Second value: [Decimal](../../sql-reference/data-types/decimal.md).
-   `result_scale` — Scale of result: [Int/UInt](../../sql-reference/data-types/int-uint.md).

**Returned value**

-   The result of division with given scale.

Type: [Decimal256](../../sql-reference/data-types/decimal.md).

**Example**

```text
┌─divideDecimal(toDecimal256(-12, 0), toDecimal32(2.1, 1), 10)─┐
│                                                -5.7142857142 │
└──────────────────────────────────────────────────────────────┘
```

**Difference from regular division:**
```sql
SELECT toDecimal64(-12, 1) / toDecimal32(2.1, 1);
SELECT toDecimal64(-12, 1) as a, toDecimal32(2.1, 1) as b, divideDecimal(a, b, 1), divideDecimal(a, b, 5);
```

```text
┌─divide(toDecimal64(-12, 1), toDecimal32(2.1, 1))─┐
│                                             -5.7 │
└──────────────────────────────────────────────────┘
┌───a─┬───b─┬─divideDecimal(toDecimal64(-12, 1), toDecimal32(2.1, 1), 1)─┬─divideDecimal(toDecimal64(-12, 1), toDecimal32(2.1, 1), 5)─┐
│ -12 │ 2.1 │                                                       -5.7 │                                                   -5.71428 │
└─────┴─────┴────────────────────────────────────────────────────────────┴────────────────────────────────────────────────────────────┘
```

```sql
SELECT toDecimal64(-12, 0) / toDecimal32(2.1, 1);
SELECT toDecimal64(-12, 0) as a, toDecimal32(2.1, 1) as b, divideDecimal(a, b, 1), divideDecimal(a, b, 5);
```

```text
DB::Exception: Decimal result's scale is less than argument's one: While processing toDecimal64(-12, 0) / toDecimal32(2.1, 1). (ARGUMENT_OUT_OF_BOUND)
┌───a─┬───b─┬─divideDecimal(toDecimal64(-12, 0), toDecimal32(2.1, 1), 1)─┬─divideDecimal(toDecimal64(-12, 0), toDecimal32(2.1, 1), 5)─┐
│ -12 │ 2.1 │                                                       -5.7 │                                                   -5.71428 │
└─────┴─────┴────────────────────────────────────────────────────────────┴────────────────────────────────────────────────────────────┘
```
)"));
}

}
