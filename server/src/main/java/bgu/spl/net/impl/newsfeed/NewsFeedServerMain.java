package bgu.spl.net.impl.newsfeed;

import bgu.spl.net.impl.rci.ObjectEncoderDecoder;
import bgu.spl.net.impl.rci.RemoteCommandInvocationProtocol;
import bgu.spl.net.srv.Server;

public class NewsFeedServerMain {

    public static void main(String[] args) {
        NewsFeed feed = new NewsFeed();
        Server.threadPerClient(
                7777,
                () -> new RemoteCommandInvocationProtocol<>(feed),
                ObjectEncoderDecoder::new
        ).serve();

        // Server.reactor(
        //         Runtime.getRuntime().availableProcessors(),
        //         7777,
        //         () ->  new RemoteCommandInvocationProtocol<>(feed),
        //         ObjectEncoderDecoder::new
        // ).serve();
    }
}