// Metodos estaticos: recursao, parametros long/double misturados e retornos de todos os tipos.
public class Recursion {
    static int fact(int n) {
        return n <= 1 ? 1 : n * fact(n - 1);
    }

    static long factLong(int n) {
        return n <= 1 ? 1L : n * factLong(n - 1);
    }

    static int fib(int n) {
        if (n < 2) {
            return n;
        }
        return fib(n - 1) + fib(n - 2);
    }

    static double mix(int a, long b, double c, float d, long e) {
        return a + b + c + d + e;
    }

    static long sumLongs(long a, int b, long c) {
        return a * 1000 + b * 10 + c;
    }

    static boolean isEven(int n) {
        return n % 2 == 0;
    }

    static char next(char c) {
        return (char) (c + 1);
    }

    static float half(float f) {
        return f / 2;
    }

    static void hello() {
        System.out.println("ola de um metodo void");
    }

    static int gcd(int a, int b) {
        return b == 0 ? a : gcd(b, a % b);
    }

    public static void main(String[] args) {
        System.out.println(fact(10));
        System.out.println(factLong(20));
        System.out.println(fib(20));
        System.out.println(mix(1, 2000000000000L, 0.5, 0.25f, -3L));
        System.out.println(sumLongs(5L, 7, 9L));
        System.out.println(isEven(10));
        System.out.println(isEven(7));
        System.out.println(next('a'));
        System.out.println(half(5.0f));
        hello();
        System.out.println(gcd(1071, 462));
    }
}
