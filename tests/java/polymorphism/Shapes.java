// Polimorfismo com classe abstrata e interface.
public class Shapes {
    static double total(Shape[] shapes) {
        double sum = 0;
        for (int i = 0; i < shapes.length; i++) {
            sum += shapes[i].area();
        }
        return sum;
    }

    public static void main(String[] args) {
        Shape[] shapes = new Shape[3];
        shapes[0] = new Rect(2, 3);
        shapes[1] = new Square(4);
        shapes[2] = new Circle(1);
        for (int i = 0; i < shapes.length; i++) {
            System.out.println(shapes[i].name() + " " + shapes[i].area());
        }
        System.out.println(total(shapes));
        Named n = new Square(2);
        System.out.println(n.name());
        System.out.println(n instanceof Shape);
        System.out.println(shapes[2] instanceof Named);
        Named[] named = new Named[2];
        named[0] = new Rect(1, 1);
        named[1] = new Circle(2);
        for (int i = 0; i < named.length; i++) {
            System.out.println(named[i].name());
        }
    }
}

interface Named {
    String name();
}

abstract class Shape implements Named {
    abstract double area();

    public String name() {
        return "forma";
    }
}

class Rect extends Shape {
    double w, h;

    Rect(double w, double h) {
        this.w = w;
        this.h = h;
    }

    double area() {
        return w * h;
    }

    public String name() {
        return "retangulo";
    }
}

class Square extends Rect {
    Square(double s) {
        super(s, s);
    }

    public String name() {
        return "quadrado";
    }
}

class Circle extends Shape {
    double r;

    Circle(double r) {
        this.r = r;
    }

    double area() {
        return 3.0 * r * r;
    }
}
