// Metodos estaticos de outra classe, sobrecarga estatica, recursao mutua e Hanoi.
public class MathUtil {
    public static void main(String[] args) {
        System.out.println(Calc.power(2, 10));
        System.out.println(Calc.power(2L, 62));
        System.out.println(Calc.power(1.5, 3));
        System.out.println(Calc.isPrime(97));
        System.out.println(Calc.isPrime(91));
        System.out.println(Calc.even(10) + " " + Calc.odd(7));
        System.out.println(Calc.hanoi(10));
        System.out.println(Calc.sum(new int[] {1, 2, 3, 4}));
        System.out.println(Calc.avg(1, 2L, 3.0f, 4.0));
        System.out.println(Calc.max(-5, -2));
        System.out.println(Calc.ackermann(2, 3));
    }
}

class Calc {
    static int power(int b, int e) {
        int r = 1;
        for (int i = 0; i < e; i++) {
            r *= b;
        }
        return r;
    }

    static long power(long b, int e) {
        return e == 0 ? 1 : b * power(b, e - 1);
    }

    static double power(double b, int e) {
        double r = 1;
        while (e-- > 0) {
            r *= b;
        }
        return r;
    }

    static boolean isPrime(int n) {
        if (n < 2) {
            return false;
        }
        for (int d = 2; d * d <= n; d++) {
            if (n % d == 0) {
                return false;
            }
        }
        return true;
    }

    static boolean even(int n) {
        return n == 0 ? true : odd(n - 1);
    }

    static boolean odd(int n) {
        return n == 0 ? false : even(n - 1);
    }

    static long hanoi(int n) {
        return n == 0 ? 0 : 2 * hanoi(n - 1) + 1;
    }

    static int sum(int[] v) {
        int s = 0;
        for (int i = 0; i < v.length; i++) {
            s += v[i];
        }
        return s;
    }

    static double avg(int a, long b, float c, double d) {
        return (a + b + c + d) / 4;
    }

    static int max(int a, int b) {
        return a > b ? a : b;
    }

    static int ackermann(int m, int n) {
        if (m == 0) return n + 1;
        if (n == 0) return ackermann(m - 1, 1);
        return ackermann(m - 1, ackermann(m, n - 1));
    }
}
