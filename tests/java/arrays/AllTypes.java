// Arrays de todos os tipos primitivos, de objetos, arraylength e valores padrao.
public class AllTypes {
    public static void main(String[] args) {
        boolean[] z = new boolean[2];
        byte[] b = new byte[3];
        char[] c = new char[3];
        short[] s = new short[2];
        int[] i = new int[4];
        long[] l = new long[2];
        float[] f = new float[2];
        double[] d = new double[2];
        String[] str = new String[2];
        z[1] = true;
        b[0] = 127; b[1] = (byte) 128; b[2] = -1;
        c[0] = 'J'; c[1] = 'V'; c[2] = 'M';
        s[0] = 32767; s[1] = (short) 40000;
        i[0] = 1; i[3] = -5;
        l[0] = 1L << 40; l[1] = -l[0];
        f[0] = 1.25f; f[1] = f[0] * 2;
        d[0] = 0.5; d[1] = d[0] / 4;
        str[0] = "a";
        System.out.println(z[0] + " " + z[1]);
        System.out.println(b[0] + " " + b[1] + " " + b[2]);
        System.out.println(c);
        System.out.println(c[1]);
        System.out.println(s[0] + " " + s[1]);
        System.out.println(i[0] + i[1] + i[2] + i[3]);
        System.out.println(l[0] + " " + l[1]);
        System.out.println(f[0] + " " + f[1]);
        System.out.println(d[0] + " " + d[1]);
        System.out.println(str[0] + " " + str[1]);
        System.out.println(z.length + b.length + c.length + s.length + i.length + l.length + f.length + d.length + str.length);
        int[] init = {5, 3, 8, 1, 9, 2};
        sort(init);
        for (int k = 0; k < init.length; k++) {
            System.out.print(init[k]);
            System.out.print(' ');
        }
        System.out.println();
        long[] copy = {1, 2, 3};
        long total = 0;
        for (int k = 0; k < copy.length; k++) {
            total += copy[k];
        }
        System.out.println(total);
        b[0] += 1;
        System.out.println(b[0]);
        c[0]++;
        System.out.println(c[0]);
        int[] empty = new int[0];
        System.out.println(empty.length);
    }

    static void sort(int[] a) {
        for (int x = 0; x < a.length; x++) {
            for (int y = 0; y < a.length - 1 - x; y++) {
                if (a[y] > a[y + 1]) {
                    int t = a[y];
                    a[y] = a[y + 1];
                    a[y + 1] = t;
                }
            }
        }
    }
}
