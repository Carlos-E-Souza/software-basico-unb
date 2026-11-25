// Excecao nao capturada: imprime a saida ate o erro e termina com codigo diferente de zero.
public class Uncaught {
    static void fail(int x) {
        if (x == 3) {
            throw new RuntimeException("falhou em " + x);
        }
    }

    public static void main(String[] args) {
        for (int i = 0; i < 5; i++) {
            System.out.println("passo " + i);
            fail(i);
        }
    }
}
