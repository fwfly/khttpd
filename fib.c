#include <linux/ktime.h>

#define MAX_LENGTH 500
static ktime_t kt;

struct BigN {
    unsigned long long lower, upper;
};

static void addBigN(struct BigN *out, struct BigN x, struct BigN y)
{
    unsigned long long lower = ~x.lower;
    out->upper = x.upper + y.upper;
    if (y.lower > lower) {
        out->upper++;
    }
    out->lower = x.lower + y.lower;
}


/* Here is an assumption is x is always bigger then y */
static void subBigN(struct BigN *out, struct BigN x, struct BigN y)
{
    out->upper = x.upper - y.upper;
    if (y.lower > x.lower) {
        x.upper--;
        out->lower = ULONG_MAX - y.lower + x.lower;
    } else {
        out->lower = x.lower - y.lower;
    }
    // printk("%lld = %lld - %lld", out->lower, x.lower, y.lower);
}

static void multiBigN64to128(struct BigN *out,
                             unsigned long long x,
                             unsigned long long y)
{
    uint32_t a = x & 0xFFFFFFFF;
    uint32_t c = x >> 32;
    uint32_t b = y & 0xFFFFFFFF;
    uint32_t d = y >> 32;
    uint64_t ab = (uint64_t) a * b;
    uint64_t bc = (uint64_t) b * c;
    uint64_t ad = (uint64_t) a * d;
    uint64_t cd = (uint64_t) c * d;
    uint64_t low = ab + (bc << 32);
    uint64_t high = cd + (bc >> 32) + (ad >> 32) + (low < ab);
    low += (ad << 32);
    high += (low < (ad << 32));
    out->lower = low;
    out->upper = high;
}

static void multiBigN(struct BigN *out, struct BigN x, struct BigN y)
{
    unsigned long long h = x.lower * y.upper + x.upper * y.lower;
    out->upper = 0;
    out->lower = 0;
    multiBigN64to128(out, x.lower, y.lower);
    x.upper += h;
    out->upper = x.upper;

}

static inline void assignBigN(struct BigN *x, struct BigN y)
{
    x->upper = y.upper;
    x->lower = y.lower;
}

static int num_bits(long long k)
{
    int num = 0;
    while (k) {
        k = k / 2;
        num++;
    }
    return num;
}


// To pass the clang-format, we comment this code.
// This function is useful when we need to verify the number to human-readable.
static void BigN_to_int(struct BigN *res, struct BigN x)
{
    unsigned long long max10 = 10000000000000000000U;
    unsigned long long idx = x.upper;
    unsigned long long max_first = ULONG_MAX / max10;
    unsigned long long max_mod = ULONG_MAX - max_first * max10;
    unsigned long long x_first = x.lower / max10;
    unsigned long long x_mod = x.lower - x_first * max10;

    res->lower = x.lower;
    while (idx) {
        // Add mod
        x_mod = x_mod + max_mod;
        int carry = 0;
        // count if it needs carry over.
        if (x_mod > max10) {
            carry = 1;
            x_mod = x_mod - max10;
        }
        res->lower = x_mod;
        // Add x_first , max_first, carry to find upper_dec
        x_first = x_first + max_first + carry;
        res->upper = x_first;
        idx--;
    }
}


static long long fast_fib_sequence(struct BigN *res, long long k)
{
    struct BigN a, b, b2, t1, t2, t2a;
    int numbits = num_bits(k);
    int count = numbits;

    a.upper = 0;
    a.lower = 0;
    b.upper = 0;
    b.lower = 1;
    b2.upper = 0;
    b2.lower = 2;
    t1.upper = 0;
    t1.lower = 0;
    t2.upper = 0;
    t2.lower = 0;
    t2a.upper = 0;
    t2a.lower = 0;

    while (count) {
        /* t1 = a*(2*b - a) */
        multiBigN(&t1, b, b2);
        subBigN(&t1, t1, a);
        multiBigN(&t1, t1, a);

        /* t2 = b^2 + a^2; */
        multiBigN(&t2, b, b);
        multiBigN(&t2a, a, a);
        addBigN(&t2, t2, t2a);

        /* a = t1; b = t2; // m *= 2 */
        assignBigN(&a, t1);
        assignBigN(&b, t2);
        // int tmpk = (k >> (count -1)) & 1;
        // printk("%d ",tmpk );
        if ((k >> (count - 1)) & 1) {
            // printk("%d : special count\n", tmpk);
            addBigN(&t1, a, b);  // t1 = a + b;
            assignBigN(&a, b);   // a = b;
            assignBigN(&b, t1);  // b = t1;
        }
        count--;
    }

    res->upper = a.upper;
    res->lower = a.lower;
    return a.lower;
}

static void fib_ktime_proxy(struct BigN *res, long long k)
{
    // kt = ktime_get();
    fast_fib_sequence(res, k);
    // kt = ktime_sub(ktime_get(), kt);
}

static void fib_strrev(unsigned char *str)
{
    int i;
    int j;
    unsigned len = strlen((const char *) str);
    for (i = 0, j = len - 1; i < j; i++, j--) {
        unsigned char a;
        a = str[i];
        str[i] = str[j];
        str[j] = a;
    }
}

static void BigN_to_String(char *str_res, struct BigN *bn)
{
    struct BigN int_bn;
    int_bn.upper = 0;
    int_bn.lower = 0;
    BigN_to_int(&int_bn, *bn);
    // BigN to  String
    int i = 0;

    if (bn->upper == 0 && bn->lower == 0) {
        str_res[0] = '0';
        str_res[1] = '\0';
    } else {

        int digit;
        while (int_bn.lower) {
            digit = int_bn.lower % 10;
            str_res[i++] = '0' + digit;
            int_bn.lower = int_bn.lower / 10;
        }

        while (int_bn.upper) {
            digit = int_bn.upper % 10;
            str_res[i++] = '0' + digit;
            int_bn.lower = int_bn.upper / 10;
        }
    }
    str_res[i] = '\0';
    fib_strrev(str_res);
    return 0;
}

static void get_fib(char *res, char *k)
{
    // string to int
    unsigned long long nk = 0;
    struct BigN res_bn;
    res_bn.upper = 0;
    res_bn.lower = 0;
    if (!kstrtoull(k + 5, 0, &nk)) {
        // calculate fib
        fib_ktime_proxy(&res_bn, nk);
    }
    // bigN to string
    BigN_to_String(res, &res_bn);
    printk("get_gib_res : %s", res);
}
