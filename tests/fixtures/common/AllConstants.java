// Fixture: gera entradas de todos os tipos numericos e de texto no pool.
public class AllConstants {
    static final int BIG_INT = 123456789;
    static final long BIG_LONG = 1234567890123L;
    static final float FLOAT_VALUE = 3.5f;
    static final double DOUBLE_VALUE = 2.718281828;
    static final String TEXT = "Ola é中\u0000😀!";
    static int counter;
    int instanceField;

    public static void main(String[] args) {
        long l = BIG_LONG + counter;
        double d = DOUBLE_VALUE * counter;
        float f = FLOAT_VALUE + counter;
        int i = BIG_INT + counter;
        String s = TEXT;
        counter = (int) (l + d + f + i + s.length());
    }
}
