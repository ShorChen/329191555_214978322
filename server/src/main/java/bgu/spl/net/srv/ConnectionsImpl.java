package bgu.spl.net.srv;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.atomic.AtomicInteger;

public class ConnectionsImpl<T> implements Connections<T> {
    private final ConcurrentHashMap<Integer, ConnectionHandler<T>> activeConnections = new ConcurrentHashMap<>();
    private final ConcurrentHashMap<String, CopyOnWriteArrayList<Integer>> channelSubscribers = new ConcurrentHashMap<>();
    private final AtomicInteger connectionIdCounter = new AtomicInteger(0);
    public int addActiveConnection(ConnectionHandler<T> handler) {
        int id = connectionIdCounter.getAndIncrement();
        activeConnections.put(id, handler);
        return id;
    }

    @Override
    public boolean send(int connectionId, T msg) {
        ConnectionHandler<T> handler = activeConnections.get(connectionId);
        if (handler != null) {
            handler.send(msg);
            return true;
        }
        return false;
    }

    @Override
    public void send(String channel, T msg) {
        CopyOnWriteArrayList<Integer> subscribers = channelSubscribers.get(channel);
        if (subscribers != null)
            for (Integer connectionId : subscribers)
                send(connectionId, msg);
    }

    @Override
    public void disconnect(int connectionId) {
        activeConnections.remove(connectionId);
        for (CopyOnWriteArrayList<Integer> subscribers : channelSubscribers.values())
            subscribers.remove(Integer.valueOf(connectionId));
    }

    public void subscribe(String channel, int connectionId) {
        channelSubscribers.computeIfAbsent(channel, k -> new CopyOnWriteArrayList<>()).add(connectionId);
    }

    public void unsubscribe(String channel, int connectionId) {
        CopyOnWriteArrayList<Integer> subscribers = channelSubscribers.get(channel);
        if (subscribers != null)
            subscribers.remove(Integer.valueOf(connectionId));
    }
}