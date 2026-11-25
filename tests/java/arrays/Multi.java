// Arrays multidimensionais: matriz 3D, matriz irregular e array de objetos com polimorfismo.
public class Multi {
    public static void main(String[] args) {
        int[][][] cube = new int[2][3][4];
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 3; j++)
                for (int k = 0; k < 4; k++)
                    cube[i][j][k] = i * 100 + j * 10 + k;
        System.out.println(cube[1][2][3]);
        System.out.println(cube.length + " " + cube[0].length + " " + cube[0][0].length);
        int[][] jagged = new int[3][];
        for (int i = 0; i < jagged.length; i++) {
            jagged[i] = new int[i + 1];
            for (int j = 0; j <= i; j++) {
                jagged[i][j] = i + j;
            }
        }
        for (int i = 0; i < jagged.length; i++) {
            String line = "";
            for (int j = 0; j < jagged[i].length; j++) {
                line = line + jagged[i][j] + " ";
            }
            System.out.println(line);
        }
        double[][] m = {{1.5, 2.5}, {3.5, 4.5}};
        System.out.println(m[1][0] + m[0][1]);
        long[][] partial = new long[2][];
        System.out.println(partial[0] == null);
        String[][] names = new String[2][2];
        names[1][1] = "x";
        System.out.println(names[1][1] + names[0][0]);
        Object[] objs = new Object[3];
        objs[0] = "string";
        objs[1] = new int[2];
        objs[2] = new Thing(7);
        System.out.println(objs[0]);
        System.out.println(objs[2]);
        System.out.println(objs[1] instanceof int[]);
        System.out.println(objs[0] instanceof String);
        Thing[] things = new Thing[2];
        Object o = things;
        System.out.println(o instanceof Object[]);
        char[][] grid = new char[2][3];
        grid[0][0] = 'a';
        grid[1][2] = 'z';
        System.out.println(grid[0][0]);
        System.out.println(grid[1][2]);
    }
}

class Thing {
    int v;

    Thing(int v) {
        this.v = v;
    }

    public String toString() {
        return "Thing" + v;
    }
}
