// Impressao: limites de cada tipo, notacao cientifica, char/boolean e mistura de print e println.
public class Formats {
    public static void main(String[] args) {
        System.out.println(2147483647);
        System.out.println(-2147483648);
        System.out.println(9223372036854775807L);
        System.out.println(1.0E-3);
        System.out.println(9.999999E-4);
        System.out.println(1.0E-4f);
        System.out.println(9999999.0);
        System.out.println(1.0E21);
        System.out.println(123.456);
        System.out.println(-0.5f);
        System.out.println(1.7976931348623157E308);
        System.out.println(Double_MIN());
        System.out.println(1.4E-45f);
        System.out.println((byte) 127);
        System.out.println((short) -32768);
        System.out.println((char) 97);
        System.out.println(true);
        for (int i = 0; i < 3; i++) {
            System.out.print("[" + i + "]");
        }
        System.out.println();
        System.out.print('x');
        System.out.print(1.5);
        System.out.print(2.5f);
        System.out.print(3L);
        System.out.print(false);
        System.out.println("!");
        System.out.println("");
        System.out.println("tab\tfim");
        System.out.println("ultimo");
    }

    static double Double_MIN() {
        return 4.9E-324;
    }
}
