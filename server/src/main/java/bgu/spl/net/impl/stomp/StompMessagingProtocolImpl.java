package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.impl.data.Database;
import bgu.spl.net.impl.data.LoginStatus;
import bgu.spl.net.srv.Connections;
import bgu.spl.net.srv.ConnectionsImpl;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public class StompMessagingProtocolImpl implements StompMessagingProtocol<String> {
    private int connectionId;
    private ConnectionsImpl<String> connections;
    private boolean shouldTerminate = false;
    private boolean isConnected = false;
    private String login = null;
    private Map<String, String> subscriptionIdToTopic = new ConcurrentHashMap<>();

    @Override
    public void start(int connectionId, Connections<String> connections) {
        this.connectionId = connectionId;
        this.connections = (ConnectionsImpl<String>) connections;
    }

    @Override
    public void process(String message) {
        System.out.println("-----------------------------------");
        System.out.println("Received Frame:");
        System.out.println(message);
        System.out.println("-----------------------------------");
        String[] lines = message.split("\n");
        if (lines.length == 0) return;
        String command = lines[0].trim();
        Map<String, String> headers = parseHeaders(lines);
        String body = parseBody(message);
        switch (command) {
            case "CONNECT":
                handleConnect(headers);
                break;
            case "SUBSCRIBE":
                handleSubscribe(headers);
                break;
            case "UNSUBSCRIBE":
                handleUnsubscribe(headers);
                break;
            case "SEND":
                handleSend(headers, body);
                break;
            case "DISCONNECT":
                handleDisconnect(headers);
                break;
            default:
                sendError("Unknown command", "The command " + command + " is not recognized.");
        }
    }

    @Override
    public boolean shouldTerminate() {
        return shouldTerminate;
    }

    private void handleConnect(Map<String, String> headers) {
        @SuppressWarnings("unused")
        String version = headers.get("accept-version");
        @SuppressWarnings("unused")
        String host = headers.get("host");
        String loginUser = headers.get("login");
        String passcode = headers.get("passcode");
        if (isConnected) {
            sendError("Already connected", "Client is already logged in.");
            return;
        }
        LoginStatus status = Database.getInstance().login(connectionId, loginUser, passcode);
        if (status == LoginStatus.WRONG_PASSWORD) {
            sendError("Wrong password", "Password does not match.");
            shouldTerminate = true;
            connections.disconnect(connectionId);
            return;
        } else if (status == LoginStatus.ALREADY_LOGGED_IN) {
            sendError("User already logged in", "User " + loginUser + " is already active.");
            shouldTerminate = true;
            connections.disconnect(connectionId);
            return;
        } else if (status == LoginStatus.CLIENT_ALREADY_CONNECTED) {
            sendError("Client already connected", "Connection is already active.");
            return;
        }        
        isConnected = true;
        login = loginUser;
        String response = "CONNECTED\n" +
                          "version:1.2\n" +
                          "\n"; 
        connections.send(connectionId, response);
    }

    private void handleSubscribe(Map<String, String> headers) {
        String dest = headers.get("destination");
        String subId = headers.get("id");
        String receipt = headers.get("receipt");
        if (!isConnected || dest == null || subId == null) {
            sendError("Malformed Frame", "Missing headers for SUBSCRIBE");
            return;
        }
        connections.subscribe(dest, connectionId, subId);
        subscriptionIdToTopic.put(subId, dest);
        if (receipt != null)
            sendReceipt(receipt);
    }

    private void handleUnsubscribe(Map<String, String> headers) {
        String subId = headers.get("id");
        String receipt = headers.get("receipt");
        if (!isConnected || subId == null) {
            sendError("Malformed Frame", "Missing id header for UNSUBSCRIBE");
            return;
        }
        String topic = subscriptionIdToTopic.remove(subId);
        if (topic != null)
            connections.unsubscribe(topic, connectionId);
        if (receipt != null)
            sendReceipt(receipt);
    }

    private void handleSend(Map<String, String> headers, String body) {
        String dest = headers.get("destination");
        String receipt = headers.get("receipt");
        String filename = headers.get("file");
        if (!isConnected || dest == null) {
            sendError("Malformed Frame", "Missing destination for SEND");
            return;
        }
        if (filename != null && login != null)
            Database.getInstance().trackFileUpload(login, filename, dest);
        String msgId = String.valueOf(System.currentTimeMillis()); 
        String responseFrame = "MESSAGE\n" +
                               "destination:" + dest + "\n" +
                               "message-id:" + msgId + "\n" +
                               "subscription:{subscription-id-placeholder}\n" +
                               "\n" +
                               body;
        connections.send(dest, responseFrame);
        if (receipt != null)
            sendReceipt(receipt);
    }

    private void handleDisconnect(Map<String, String> headers) {
        String receipt = headers.get("receipt");
        if (receipt != null)
            sendReceipt(receipt);
        if (login != null)
            Database.getInstance().logout(connectionId);
        shouldTerminate = true;
        isConnected = false;
        connections.disconnect(connectionId);
    }

    private void sendReceipt(String receiptId) {
        String frame = "RECEIPT\n" +
                       "receipt-id:" + receiptId + "\n" +
                       "\n";
        connections.send(connectionId, frame);
    }

    private void sendError(String message, String description) {
        String frame = "ERROR\n" +
                       "message:" + message + "\n" +
                       "\n" +
                       description;
        connections.send(connectionId, frame);
        shouldTerminate = true;
        connections.disconnect(connectionId);
    }

    private Map<String, String> parseHeaders(String[] lines) {
        Map<String, String> headers = new ConcurrentHashMap<>();
        for (int i = 1; i < lines.length; i++) {
            String line = lines[i].trim();
            if (line.isEmpty()) break; 
            String[] parts = line.split(":", 2);
            if (parts.length == 2)
                headers.put(parts[0], parts[1]);
        }
        return headers;
    }

    private String parseBody(String message) {
        int splitIndex = message.indexOf("\n\n");
        if (splitIndex == -1) return "";
        return message.substring(splitIndex + 2);
    }
}