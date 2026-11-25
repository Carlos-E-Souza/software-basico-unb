// Reescrita de metodos, sobrecarga, toString/equals reescritos e chamadas via super.
public class Override {
    public static void main(String[] args) {
        Base b = new Derived();
        System.out.println(b.who());
        System.out.println(b.callWho());
        System.out.println(b.over(1));
        System.out.println(b.over(1L));
        System.out.println(b.over("s"));
        System.out.println(b);
        Derived d = new Derived();
        System.out.println(d.equals(new Derived()));
        System.out.println(d.equals("x"));
        System.out.println(d.superWho());
        System.out.println(new Leaf().who());
    }
}

class Base {
    String who() {
        return "Base";
    }

    String callWho() {
        return "chamou " + who();
    }

    String over(int x) {
        return "int " + x;
    }

    String over(long x) {
        return "long " + x;
    }

    String over(String x) {
        return "String " + x;
    }

    public String toString() {
        return "Base.toString -> " + who();
    }
}

class Derived extends Base {
    String who() {
        return "Derived";
    }

    String over(long x) {
        return "Derived long " + x;
    }

    String superWho() {
        return super.who() + "/" + who();
    }

    public boolean equals(Object o) {
        return o instanceof Derived;
    }
}

class Leaf extends Derived {
    String who() {
        return "Leaf(" + super.who() + ")";
    }
}
