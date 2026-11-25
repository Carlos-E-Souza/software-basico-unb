// Reescrita: metodo template na classe abstrata chamando metodos reescritos, construtor chamando metodo virtual.
public class Templates {
    public static void main(String[] args) {
        Report[] rs = new Report[2];
        rs[0] = new SalesReport();
        rs[1] = new StockReport();
        for (int i = 0; i < rs.length; i++) {
            rs[i].generate();
        }
        Report r = new StockReport();
        System.out.println(r.title());
        System.out.println(new Base2().value() + " " + new Child2().value());
        Base2 b = new Child2();
        System.out.println(b.describe());
    }
}

abstract class Report {
    final void generate() {
        System.out.println("== " + title() + " ==");
        body();
        System.out.println(footer());
    }

    abstract String title();

    abstract void body();

    String footer() {
        return "-- fim --";
    }
}

class SalesReport extends Report {
    String title() {
        return "Vendas";
    }

    void body() {
        System.out.println("total: " + 1234.5);
    }
}

class StockReport extends Report {
    String title() {
        return "Estoque";
    }

    void body() {
        System.out.println("itens: " + 42);
    }

    String footer() {
        return "-- estoque ok --";
    }
}

class Base2 {
    int v = 1;

    Base2() {
        System.out.println("construtor Base2 chama value(): " + value());
    }

    int value() {
        return 10;
    }

    String describe() {
        return "value=" + value();
    }
}

class Child2 extends Base2 {
    int w = 5;

    int value() {
        return 20 + w;
    }
}
