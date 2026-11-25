// Impressao de todos os tipos com print e println, incluindo NaN, Infinity e notacao cientifica.
public class PrintAll {
    public static void main(String[] args) {
        System.out.print(1);
        System.out.print(' ');
        System.out.print(2L);
        System.out.print(' ');
        System.out.print(3.0f);
        System.out.print(' ');
        System.out.print(4.0);
        System.out.print(' ');
        System.out.print(true);
        System.out.print(' ');
        System.out.print("str");
        System.out.println();
        System.out.println(Integer_MIN());
        System.out.println(-9223372036854775808L);
        System.out.println(0.0f / 0);
        System.out.println(1.0f / 0);
        System.out.println(-1.0 / 0);
        System.out.println(1.0E10);
        System.out.println(1.0E-10);
        System.out.println(0.001);
        System.out.println(0.0001);
        System.out.println(1234567.0);
        System.out.println(12345678.0);
        System.out.println(1.0E7f);
        System.out.println(3.14159265358979);
        System.out.println(2.718281828f);
        System.out.println('A');
        System.out.println((char) 0x263A);
        char[] word = {'J', 'a', 'v', 'a'};
        System.out.println(word);
        System.out.println(false);
        Object o = "objeto";
        System.out.println(o);
        Object none = null;
        System.out.println(none);
        String ns = null;
        System.out.println(ns);
        byte b = -3;
        short sh = 300;
        System.out.println(b);
        System.out.println(sh);
    }

    static int Integer_MIN() {
        return -2147483648;
    }
}
