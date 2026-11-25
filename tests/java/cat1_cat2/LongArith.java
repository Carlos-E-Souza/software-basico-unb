// Aritmetica de long (categoria 2): limites, divisao, deslocamentos e comparacoes.
public class LongArith {
    public static void main(String[] args) {
        long max = 9223372036854775807L;
        long min = -9223372036854775808L;
        System.out.println(max + 1);
        System.out.println(min / -1);
        System.out.println(min % -1);
        System.out.println(max * 3);
        System.out.println(-7L / 2);
        System.out.println(-7L % 2);
        System.out.println(1L << 65);
        System.out.println(-256L >> 4);
        System.out.println(-1L >>> 60);
        System.out.println(0xFF00FF00FF00L & 0x0FF00FF00FF0L);
        System.out.println(5L | 10L);
        System.out.println(6L ^ 3L);
        System.out.println(-max);
        long a = 123456789012L;
        long b = 98765L;
        System.out.println(a * b);
        System.out.println(a / b);
        System.out.println(a % b);
        System.out.println(a > b);
        System.out.println(a < b);
        System.out.println(a == b);
        int x = 100000;
        long y = x * (long) x;
        System.out.println(y);
        System.out.println((int) 4294967297L);
        long counter = 0;
        for (int i = 0; i < 10; i++) {
            counter += i * 1000000000L;
        }
        System.out.println(counter);
    }
}
