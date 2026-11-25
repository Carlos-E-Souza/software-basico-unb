// Hierarquia de excecoes do usuario e captura pela superclasse.
public class UserException {
    static void check(int v) throws AppException {
        if (v < 0) {
            throw new NegativeException(v);
        }
        if (v == 0) {
            throw new AppException("zero");
        }
    }

    public static void main(String[] args) {
        int[] values = {5, 0, -2};
        for (int i = 0; i < values.length; i++) {
            try {
                check(values[i]);
                System.out.println("ok " + values[i]);
            } catch (NegativeException e) {
                System.out.println("negativo: " + e.getMessage() + " / " + e.value);
            } catch (AppException e) {
                System.out.println("app: " + e.getMessage());
            } finally {
                System.out.println("verificou " + values[i]);
            }
        }
        try {
            throw new NegativeException(-1);
        } catch (Exception e) {
            System.out.println(e.toString());
        }
    }
}

class AppException extends Exception {
    AppException(String message) {
        super(message);
    }
}

class NegativeException extends AppException {
    int value;

    NegativeException(int value) {
        super("valor " + value);
        this.value = value;
    }
}
