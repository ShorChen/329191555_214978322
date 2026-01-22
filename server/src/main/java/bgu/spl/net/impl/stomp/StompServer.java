package bgu.spl.net.impl.stomp;

import java.util.Scanner;
import bgu.spl.net.impl.data.Database;
import bgu.spl.net.srv.Server;

public class StompServer {

    public static void main(String[] args) {
        if (args.length < 2) {
            System.out.println("Usage: StompServer <port> <server-type>");
            System.out.println("server-type: tpc (Thread Per Client) or reactor");
            return;
        }
        Thread inputThread = new Thread(() -> {
            try (Scanner scanner = new Scanner(System.in)) {
                System.out.println("Server is running. Type 'report' to see database stats.");
                while (scanner.hasNextLine()) {
                    String line = scanner.nextLine();
                    if (line.equalsIgnoreCase("report"))
                        Database.getInstance().printReport();
                }
            }
        });
        inputThread.start();
        int port = Integer.parseInt(args[0]);
        String serverType = args[1];
        if (serverType.equals("tpc"))
            Server.threadPerClient(
                port,
                () -> new StompMessagingProtocolImpl(),
                () -> new StompEncoderDecoder()
            ).serve();
        else if (serverType.equals("reactor"))
            Server.reactor(
                Runtime.getRuntime().availableProcessors(),
                port,
                () -> new StompMessagingProtocolImpl(),
                () -> new StompEncoderDecoder()
            ).serve();
        else System.out.println("Unknown server type. Use 'tpc' or 'reactor'.");
    }
}