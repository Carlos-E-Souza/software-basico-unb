// Polimorfismo: varias interfaces, interface que estende interface, despacho dinamico em arrays e parametros.
public class Payments {
    static double totalFees(Payment[] ps) {
        double t = 0;
        for (int i = 0; i < ps.length; i++) {
            t += ps[i].fee();
        }
        return t;
    }

    static void describe(Object o) {
        if (o instanceof Refundable) {
            System.out.println("reembolsavel: " + ((Refundable) o).refund());
        } else {
            System.out.println("nao reembolsavel: " + o);
        }
    }

    public static void main(String[] args) {
        Payment[] ps = new Payment[3];
        ps[0] = new Card(100);
        ps[1] = new Cash(50);
        ps[2] = new Pix(200);
        for (int i = 0; i < ps.length; i++) {
            System.out.println(ps[i].name() + " taxa " + ps[i].fee());
            describe(ps[i]);
        }
        System.out.println(totalFees(ps));
        Named n = ps[2];
        System.out.println(n.name());
        System.out.println(ps[0] instanceof Named);
    }
}

interface Named {
    String name();
}

interface Payment extends Named {
    double fee();
}

interface Refundable {
    long refund();
}

class Card implements Payment, Refundable {
    int amount;

    Card(int amount) {
        this.amount = amount;
    }

    public String name() {
        return "cartao";
    }

    public double fee() {
        return amount * 0.03;
    }

    public long refund() {
        return amount;
    }

    public String toString() {
        return "Card";
    }
}

class Cash implements Payment {
    int amount;

    Cash(int amount) {
        this.amount = amount;
    }

    public String name() {
        return "dinheiro";
    }

    public double fee() {
        return 0;
    }

    public String toString() {
        return "Cash(" + amount + ")";
    }
}

class Pix extends Card {
    Pix(int amount) {
        super(amount);
    }

    public String name() {
        return "pix";
    }

    public double fee() {
        return 0.5;
    }

    public long refund() {
        return amount - 1;
    }
}
