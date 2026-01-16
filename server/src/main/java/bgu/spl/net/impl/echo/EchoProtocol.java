package bgu.spl.net.impl.echo;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Connections;
import java.time.LocalDateTime;

public class EchoProtocol implements StompMessagingProtocol<String> {
    private int connectionId;
    private Connections<String> connections;

    @Override
    public void start(int connectionId, Connections<String> connections) {
        this.connectionId = connectionId;
        this.connections = connections;
    }

    @Override
    public void process(String msg) {
        String response = createEcho(msg);
        connections.send(connectionId, response);
    }

    @Override
    public boolean shouldTerminate() {
        return false;
    }

    private String createEcho(String message) {
        String dt = LocalDateTime.now().toString();
        return dt + " " + message; 
    }
}