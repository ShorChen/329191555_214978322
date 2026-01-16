package bgu.spl.net.impl.rci;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Connections;
import java.io.Serializable;

public class RemoteCommandInvocationProtocol<T> implements StompMessagingProtocol<Serializable> {
    private T arg;
    private int connectionId;
    private Connections<Serializable> connections;

    public RemoteCommandInvocationProtocol(T arg) {
        this.arg = arg;
    }

    @Override
    public void start(int connectionId, Connections<Serializable> connections) {
        this.connectionId = connectionId;
        this.connections = connections;
    }

    @SuppressWarnings("unchecked")
    @Override
    public void process(Serializable msg) {
        Serializable result = ((Command<T>) msg).execute(arg);
        if (result != null)
            connections.send(connectionId, result);
    }

    @Override
    public boolean shouldTerminate() {
        return false;
    }
}