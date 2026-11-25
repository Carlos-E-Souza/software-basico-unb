// Atributos de instancia: arrays, long/double e referencias a outros objetos, independentes por objeto.
public class Matrix {
    int rows;
    int cols;
    double[][] cells;
    long version;
    Matrix previous;

    Matrix(int rows, int cols) {
        this.rows = rows;
        this.cols = cols;
        cells = new double[rows][cols];
    }

    void set(int r, int c, double v) {
        cells[r][c] = v;
        version++;
    }

    Matrix multiply(Matrix o) {
        Matrix m = new Matrix(rows, o.cols);
        for (int i = 0; i < rows; i++)
            for (int j = 0; j < o.cols; j++) {
                double s = 0;
                for (int k = 0; k < cols; k++) {
                    s += cells[i][k] * o.cells[k][j];
                }
                m.cells[i][j] = s;
            }
        m.previous = this;
        return m;
    }

    void print() {
        for (int i = 0; i < rows; i++) {
            String line = "";
            for (int j = 0; j < cols; j++) {
                line += cells[i][j] + " ";
            }
            System.out.println(line);
        }
    }

    public static void main(String[] args) {
        Matrix a = new Matrix(2, 2);
        Matrix b = new Matrix(2, 2);
        a.set(0, 0, 1); a.set(0, 1, 2); a.set(1, 0, 3); a.set(1, 1, 4);
        b.set(0, 0, 0.5); b.set(1, 1, 2);
        Matrix c = a.multiply(b);
        c.print();
        System.out.println(a.version + " " + b.version + " " + c.version);
        System.out.println(c.previous == a);
        System.out.println(c.previous.cells[1][1]);
        a.cells[0][0] = 100;
        System.out.println(b.cells[0][0] + " " + c.previous.cells[0][0]);
    }
}
