// Metodos de instancia: estado, encadeamento e objetos passados como parametro.
public class Account {
    private long balance;
    private String owner;
    private int ops;

    Account(String owner) {
        this.owner = owner;
    }

    Account deposit(long amount) {
        balance += amount;
        ops++;
        return this;
    }

    boolean withdraw(long amount) {
        if (amount > balance) {
            return false;
        }
        balance -= amount;
        ops++;
        return true;
    }

    void transfer(Account to, long amount) {
        if (withdraw(amount)) {
            to.deposit(amount);
        }
    }

    long getBalance() {
        return balance;
    }

    public String toString() {
        return owner + ": " + balance + " (" + ops + " ops)";
    }

    public static void main(String[] args) {
        Account a = new Account("Ana");
        Account b = new Account("Bia");
        a.deposit(100).deposit(50).deposit(25);
        System.out.println(a);
        System.out.println(a.withdraw(1000));
        a.transfer(b, 75);
        System.out.println(a);
        System.out.println(b);
        System.out.println(a.getBalance() + b.getBalance());
    }
}
