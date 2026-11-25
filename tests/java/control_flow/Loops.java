// Lacos, condicionais e switch denso (tableswitch) e esparso (lookupswitch).
public class Loops {
    static String dense(int v) {
        switch (v) {
            case 0: return "zero";
            case 1: return "um";
            case 2: return "dois";
            case 3: return "tres";
            case 4: return "quatro";
            default: return "outro";
        }
    }

    static int sparse(int v) {
        switch (v) {
            case -1000: return 1;
            case 7: return 2;
            case 1000: return 3;
            case 123456: return 4;
            default: return 0;
        }
    }

    public static void main(String[] args) {
        int sum = 0;
        for (int i = 1; i <= 100; i++) {
            sum += i;
        }
        System.out.println(sum);
        int n = 0;
        while (n < 5) {
            n++;
        }
        System.out.println(n);
        do {
            n--;
        } while (n > 2);
        System.out.println(n);
        for (int i = -1; i <= 5; i++) {
            System.out.println(dense(i));
        }
        System.out.println(sparse(-1000));
        System.out.println(sparse(7));
        System.out.println(sparse(1000));
        System.out.println(sparse(123456));
        System.out.println(sparse(8));
        int count = 0;
        for (int i = 0; i < 10; i++) {
            if (i == 3) continue;
            if (i == 8) break;
            count++;
        }
        System.out.println(count);
        outer:
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++) {
                if (i * j == 6) {
                    System.out.println("achou " + i + " " + j);
                    break outer;
                }
            }
        }
        boolean t = true, f = false;
        System.out.println(t && !f);
        System.out.println(t || f);
        System.out.println(t ^ t);
        int x = 5;
        String r = x > 3 ? "maior" : "menor";
        System.out.println(r);
    }
}
