// Aritmetica de int: overflow, divisao, resto, deslocamentos e operacoes logicas.
public class IntArith {
    public static void main(String[] args) {
        int max = 2147483647;
        int min = -2147483648;
        System.out.println(max + 1);
        System.out.println(min - 1);
        System.out.println(max * 2);
        System.out.println(min / -1);
        System.out.println(min % -1);
        System.out.println(-7 / 2);
        System.out.println(-7 % 2);
        System.out.println(7 % -3);
        System.out.println(1 << 33);
        System.out.println(-16 >> 2);
        System.out.println(-16 >>> 28);
        System.out.println(0xF0 & 0x3C);
        System.out.println(0xF0 | 0x0F);
        System.out.println(0xFF ^ 0x0F);
        System.out.println(-max);
        System.out.println(~5);
        int i = 10;
        i += 5; i -= 3; i *= 2; i /= 5; i %= 3;
        System.out.println(i);
        i = 0;
        i++; i++; i--;
        System.out.println(i);
        int big = 1000;
        big += 30000;
        System.out.println(big);
        byte b = (byte) 200;
        short s = (short) 70000;
        char c = (char) 65601;
        System.out.println(b);
        System.out.println(s);
        System.out.println(c);
        System.out.println((int) c);
    }
}
