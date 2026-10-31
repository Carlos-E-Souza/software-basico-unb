// Fixture: heranca, classe abstrata e interfaces.
interface Shape {
    double area();
}

interface Named {
    String name();
}

abstract class Base {
    protected int id;

    abstract void run();
}

public class Hierarchy extends Base implements Shape, Named {
    private double side = 2.0;

    public double area() {
        return side * side;
    }

    public String name() {
        return "quadrado";
    }

    void run() {
        id++;
    }
}
