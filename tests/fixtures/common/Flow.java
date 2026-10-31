// Fixture: excecoes, finally, switch, lacos, arrays e classes internas.
public class Flow {
    static class Failure extends Exception {
        Failure(String message) {
            super(message);
        }
    }

    class Inner {
        int value() {
            return 42;
        }
    }

    static int divide(int a, int b) throws Failure {
        if (b == 0) {
            throw new Failure("divisao por zero");
        }
        return a / b;
    }

    static int dense(int x) {
        switch (x) {
            case 1: return 10;
            case 2: return 20;
            case 3: return 30;
            default: return 0;
        }
    }

    static int sparse(int x) {
        switch (x) {
            case 1: return 1;
            case 1000: return 2;
            case 100000: return 3;
            default: return -1;
        }
    }

    static int run() {
        int total = 0;
        int[][] grid = new int[3][4];
        long[] longs = new long[2];
        for (int i = 0; i < 3; i++) {
            try {
                total += divide(10, i);
            } catch (Failure e) {
                total -= 1;
            } finally {
                total += 100;
            }
        }
        grid[1][2] = total;
        longs[0] = grid[1][2];
        return (int) longs[0] + dense(2) + sparse(1000);
    }
}
