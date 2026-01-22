package bgu.spl.net.srv;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.atomic.AtomicInteger;

public class ConnectionsImpl<T> implements Connections<T> {
    private final ConcurrentHashMap<Integer, ConnectionHandler<T>> activeConnections = new ConcurrentHashMap<>();
    private final ConcurrentHashMap<String, CopyOnWriteArrayList<Integer>> channelSubscribers = new ConcurrentHashMap<>();
    private final ConcurrentHashMap<Integer, ConcurrentHashMap<String, String>> clientSubscriptionIds = new ConcurrentHashMap<>();
    private final AtomicInteger connectionIdCounter = new AtomicInteger(0);

    public int addActiveConnection(ConnectionHandler<T> handler) {
        int id = connectionIdCounter.getAndIncrement();
        activeConnections.put(id, handler);
        clientSubscriptionIds.put(id, new ConcurrentHashMap<>());
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
    @SuppressWarnings("unchecked")
    public void send(String channel, T msg) {
        CopyOnWriteArrayList<Integer> subscribers = channelSubscribers.get(channel);
        if (subscribers != null)
            for (Integer connectionId : subscribers) {
                String subId = clientSubscriptionIds.get(connectionId).get(channel);
                if (subId != null) {
                    String finalMsg = ((String)msg).replace("{subscription-id-placeholder}", subId);
                    send(connectionId, (T)finalMsg);
            }
        }
    }

    @Override
    public void disconnect(int connectionId) {
        activeConnections.remove(connectionId);
        clientSubscriptionIds.remove(connectionId);
        for (CopyOnWriteArrayList<Integer> subscribers : channelSubscribers.values())
            subscribers.remove(Integer.valueOf(connectionId));
    }

    public void subscribe(String channel, int connectionId, String subscriptionId) {
        channelSubscribers.computeIfAbsent(channel, k -> new CopyOnWriteArrayList<>()).add(connectionId);
        clientSubscriptionIds.computeIfAbsent(connectionId, k -> new ConcurrentHashMap<>()).put(channel, subscriptionId);
    }

    public void unsubscribe(String channel, int connectionId) {
        CopyOnWriteArrayList<Integer> subscribers = channelSubscribers.get(channel);
        if (subscribers != null)
            subscribers.remove(Integer.valueOf(connectionId));
        if (clientSubscriptionIds.containsKey(connectionId))
            clientSubscriptionIds.get(connectionId).remove(channel);
    }
}