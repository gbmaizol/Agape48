package dk.geeak.agape48;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;

/**
 * Storage Access Framework bridge.
 *
 * Agape48 syncs the HP 48 state by letting the user point it at a folder that
 * some other app already syncs. On Android that folder arrives as a content://
 * TREE uri from ACTION_OPEN_DOCUMENT_TREE, which has no POSIX path, so the C
 * emulator core cannot fopen() it. What it can use is a file descriptor, so
 * everything here funnels down to openFd(): resolve or create the document,
 * open it read-write, and detach the fd so it outlives this Java object.
 *
 * All of this stays on the Java side deliberately - doing it through JNI from
 * C++ would be three times the code and would pull in Qt's private Android
 * headers.
 */
public final class SafBridge {

    private static final int REQUEST_PICK_TREE = 0x48;

    private SafBridge() {}

    /** Set by the Qt activity at startup. */
    public static Activity activity = null;

    /** Implemented in C++ and registered with registerNativeMethods(). */
    public static native void onTreePicked(String treeUri);

    public static void pickTree() {
        if (activity == null) return;
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                 | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                 | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        activity.startActivityForResult(i, REQUEST_PICK_TREE);
    }

    /** Call from the activity's onActivityResult. */
    public static void handleResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != REQUEST_PICK_TREE || resultCode != Activity.RESULT_OK
                || data == null || data.getData() == null) {
            return;
        }
        Uri tree = data.getData();
        // Without this the grant dies with the process and the user has to
        // re-pick the folder on every launch.
        activity.getContentResolver().takePersistableUriPermission(tree,
                Intent.FLAG_GRANT_READ_URI_PERMISSION
              | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        onTreePicked(tree.toString());
    }

    /**
     * Opens (creating if absent) {@code name} inside the tree and returns a
     * detached file descriptor, or -1.
     */
    public static int openFd(String treeUri, String name) {
        if (activity == null) return -1;
        try {
            ContentResolver cr = activity.getContentResolver();
            Uri tree = Uri.parse(treeUri);
            Uri docId = DocumentsContract.buildDocumentUriUsingTree(
                    tree, DocumentsContract.getTreeDocumentId(tree));

            Uri file = findChild(cr, tree, docId, name);
            if (file == null) {
                file = DocumentsContract.createDocument(
                        cr, docId, "application/octet-stream", name);
            }
            if (file == null) return -1;

            ParcelFileDescriptor pfd = cr.openFileDescriptor(file, "rw");
            if (pfd == null) return -1;
            return pfd.detachFd();
        } catch (Exception e) {
            return -1;
        }
    }

    public static boolean isWritable(String treeUri) {
        if (activity == null) return false;
        try {
            for (android.content.UriPermission p
                    : activity.getContentResolver().getPersistedUriPermissions()) {
                if (p.getUri().toString().equals(treeUri) && p.isWritePermission())
                    return true;
            }
        } catch (Exception ignored) {}
        return false;
    }

    public static String displayName(String treeUri) {
        if (activity == null) return treeUri;
        try {
            Uri tree = Uri.parse(treeUri);
            Uri doc = DocumentsContract.buildDocumentUriUsingTree(
                    tree, DocumentsContract.getTreeDocumentId(tree));
            try (Cursor c = activity.getContentResolver().query(doc,
                    new String[]{ DocumentsContract.Document.COLUMN_DISPLAY_NAME },
                    null, null, null)) {
                if (c != null && c.moveToFirst()) return c.getString(0);
            }
        } catch (Exception ignored) {}
        return treeUri;
    }

    private static Uri findChild(ContentResolver cr, Uri tree, Uri parent, String name) {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(
                tree, DocumentsContract.getDocumentId(parent));
        try (Cursor c = cr.query(children, new String[]{
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME }, null, null, null)) {
            if (c == null) return null;
            while (c.moveToNext()) {
                if (name.equals(c.getString(1)))
                    return DocumentsContract.buildDocumentUriUsingTree(tree, c.getString(0));
            }
        } catch (Exception ignored) {}
        return null;
    }
}
