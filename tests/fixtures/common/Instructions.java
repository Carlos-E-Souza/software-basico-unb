// Fixture: exercita o maior numero possivel de instrucoes diferentes
// (aritmetica e conversoes de todos os tipos, comparacoes, arrays de todos os
// tipos, manipulacao de pilha, objetos, monitores e iinc estendido com wide).
public class Instructions {
    static long staticLong;
    double instanceDouble;
    Object lock = new Object();

    static int ints(int a, int b) {
        int r = a + b - a * b / (b | 1) % 7;
        r = -r ^ (a & b) | (a << 3) >> 1 >>> 2;
        r += 1000;              // iinc estendido (wide)
        r -= 5;
        return r;
    }

    static long longs(long a, long b) {
        long r = a + b - a * b / (b | 1) % 7;
        r = -r ^ (a & b) | (a << 3) >> 1 >>> 2;
        return r > a ? r : (r < b ? b : a);
    }

    static double floats(float f, double d) {
        float g = f * 2.5f - f / 3 + f % 2;
        double e = d * 1e10 - d / 3 + d % 2 + (-d);
        if (g > f || e < d || g == 0 || e != e) {
            return g;
        }
        return e + g;
    }

    static int conversions(int i, long l, float f, double d) {
        long a = i;
        float b = i + l;
        double c = l + f;
        int x = (int) l + (int) f + (int) d;
        long y = (long) f + (long) d;
        float z = (float) d;
        byte bt = (byte) x;
        char ch = (char) x;
        short sh = (short) x;
        return (int) (a + b + c + x + y + z + bt + ch + sh);
    }

    static int arrays() {
        boolean[] z = new boolean[2];
        byte[] b = new byte[2];
        char[] c = new char[2];
        short[] s = new short[2];
        int[] i = new int[2];
        long[] l = new long[2];
        float[] f = new float[2];
        double[] d = new double[2];
        String[] t = new String[2];
        z[0] = true; b[0] = 1; c[0] = 'a'; s[0] = 2; i[0] = 3;
        l[0] = 4; f[0] = 5; d[0] = 6; t[0] = "x";
        l[1] = l[0]++;          // dup2_x2 com long
        i[1] = i[0]++;          // dup_x2
        return (z[0] ? 1 : 0) + b[0] + c[0] + s[0] + i[0] + (int) l[0]
            + (int) f[0] + (int) d[0] + t[0].length() + t.length;
    }

    long stack(long v) {
        instanceDouble = staticLong = v;    // dup2_x1
        return staticLong;
    }

    int objects(Object o) {
        synchronized (lock) {
            if (o instanceof String) {
                return ((String) o).length();
            }
            return o == null ? -1 : (o == lock ? 1 : 0);
        }
    }
}
