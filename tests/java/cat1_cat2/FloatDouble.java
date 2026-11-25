// float e double: operacoes, NaN, infinitos, conversoes e comparacoes.
public class FloatDouble {
    public static void main(String[] args) {
        float f = 1.5f;
        double d = 2.25;
        System.out.println(f + 2.0f);
        System.out.println(f * 3);
        System.out.println(d / 3);
        System.out.println(10.0 / 4);
        System.out.println(0.1 + 0.2);
        System.out.println(0.1f + 0.2f);
        System.out.println(1.0 / 0);
        System.out.println(-1.0 / 0);
        System.out.println(0.0 / 0);
        System.out.println(5.5 % 2);
        System.out.println(-5.5f % 2);
        System.out.println(1.0E10);
        System.out.println(1.0E-5);
        System.out.println(12345.678f);
        System.out.println(100.0);
        System.out.println(-0.0);
        System.out.println(3.4028235E38f);
        System.out.println(4.9E-324);
        double nan = 0.0 / 0;
        System.out.println(nan == nan);
        System.out.println(nan < 1);
        System.out.println(nan > 1);
        System.out.println((int) nan);
        System.out.println((int) 1e20);
        System.out.println((int) -1e20);
        System.out.println((long) 1e30);
        System.out.println((long) -2.9);
        System.out.println((int) 3.99f);
        System.out.println((float) 1.1);
        System.out.println((double) 1.1f);
        System.out.println((float) 1234567L);
        System.out.println((double) 9007199254740993L);
        System.out.println(-f);
        System.out.println(d > f);
        System.out.println(f < d);
        float acc = 0;
        for (int i = 0; i < 10; i++) {
            acc += 0.1f;
        }
        System.out.println(acc);
        double dacc = 0;
        for (int i = 0; i < 10; i++) {
            dacc += 0.1;
        }
        System.out.println(dacc);
    }
}
