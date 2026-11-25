// Objetos: construtores, campos de instancia, metodos de instancia, this, referencias e null.
public class Point {
    int x;
    int y;
    long id;
    double weight;
    String label;
    Point next;

    Point(int x, int y) {
        this.x = x;
        this.y = y;
        this.id = 10000000000L + x;
        this.weight = x * 0.5;
    }

    Point() {
        this(0, 0);
        label = "origem";
    }

    int manhattan() {
        return abs(x) + abs(y);
    }

    static int abs(int v) {
        return v < 0 ? -v : v;
    }

    void move(int dx, int dy) {
        x += dx;
        y += dy;
    }

    Point add(Point other) {
        return new Point(x + other.x, y + other.y);
    }

    public String toString() {
        return "(" + x + ", " + y + ")";
    }

    public static void main(String[] args) {
        Point p = new Point(3, -4);
        System.out.println(p.manhattan());
        p.move(1, 1);
        System.out.println(p);
        System.out.println(p.id);
        System.out.println(p.weight);
        System.out.println(p.label);
        Point o = new Point();
        System.out.println(o.label + " " + o);
        Point q = p.add(new Point(10, 10));
        System.out.println(q.toString());
        p.next = q;
        System.out.println(p.next.x);
        System.out.println(p.next.next == null);
        Point alias = p;
        alias.x = 99;
        System.out.println(p.x);
        System.out.println(alias == p);
        System.out.println(p.equals(q));
        System.out.println(p.equals(alias));
        Object obj = p;
        System.out.println(obj instanceof Point);
        System.out.println("ponto: " + obj);
    }
}
