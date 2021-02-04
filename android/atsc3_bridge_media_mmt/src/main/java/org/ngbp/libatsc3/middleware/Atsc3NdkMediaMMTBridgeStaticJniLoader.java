package org.ngbp.libatsc3.middleware;

import android.util.Log;

import java.nio.ByteBuffer;

public abstract class Atsc3NdkMediaMMTBridgeStaticJniLoader {

    public native int init(ByteBuffer fragmentBuffer, int maxFragmentCount);
    public native void release();

    static {
        Log.w("Atsc3NdkMediaMMTBridgeStaticJniLoader", "loading libatsc3_bridge_media_mmt");
        System.loadLibrary("atsc3_bridge_media_mmt");
    }
}
