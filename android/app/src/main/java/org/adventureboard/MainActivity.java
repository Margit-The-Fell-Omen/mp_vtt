package org.adventureboard;

import android.app.NativeActivity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.view.KeyEvent;
import android.view.inputmethod.InputMethodManager;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.concurrent.LinkedBlockingQueue;

public class MainActivity extends NativeActivity {
    private static final int PICK_NOTES = 100;
    private static final int PICK_IMAGES = 101;
    private static volatile ContentResolver sResolver;

    private final LinkedBlockingQueue<Integer> unicodeCharacterQueue =
        new LinkedBlockingQueue<>();
    private volatile int pendingFolderKind = -1;
    private volatile String pendingFolderUri = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        sResolver = getContentResolver();
        super.onCreate(savedInstanceState);
    }

    public void showSoftInput() {
        runOnUiThread(() -> {
            InputMethodManager imm = (InputMethodManager)
                getSystemService(Context.INPUT_METHOD_SERVICE);
            if (imm != null) {
                imm.showSoftInput(getWindow().getDecorView(), 0);
            }
        });
    }

    public void hideSoftInput() {
        runOnUiThread(() -> {
            InputMethodManager imm = (InputMethodManager)
                getSystemService(Context.INPUT_METHOD_SERVICE);
            if (imm != null) {
                imm.hideSoftInputFromWindow(
                    getWindow().getDecorView().getWindowToken(), 0);
            }
        });
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            unicodeCharacterQueue.offer(event.getUnicodeChar(event.getMetaState()));
        }
        return super.dispatchKeyEvent(event);
    }

    public int pollUnicodeChar() {
        Integer value = unicodeCharacterQueue.poll();
        return value == null ? 0 : value;
    }

    public void pickFolder(int kind) {
        final int requestCode = kind == 1 ? PICK_IMAGES : PICK_NOTES;
        runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            startActivityForResult(intent, requestCode);
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if ((requestCode == PICK_NOTES || requestCode == PICK_IMAGES)
                && resultCode == RESULT_OK && data != null) {
            Uri uri = data.getData();
            if (uri != null) {
                int flags = data.getFlags()
                    & (Intent.FLAG_GRANT_READ_URI_PERMISSION
                       | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                try {
                    getContentResolver().takePersistableUriPermission(uri, flags);
                } catch (Exception ignored) {
                }
                pendingFolderKind = requestCode == PICK_IMAGES ? 1 : 0;
                pendingFolderUri = uri.toString();
            }
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    public String takePickedFolder() {
        final String uri = pendingFolderUri;
        if (uri == null) {
            return null;
        }
        final int kind = pendingFolderKind;
        pendingFolderUri = null;
        pendingFolderKind = -1;
        return kind + "\n" + uri;
    }

    public static String rootDocumentUri(String treeUri) {
        if (sResolver == null) {
            return null;
        }
        try {
            Uri tree = Uri.parse(treeUri);
            return DocumentsContract.buildDocumentUriUsingTree(
                tree, DocumentsContract.getTreeDocumentId(tree)).toString();
        } catch (Exception e) {
            return null;
        }
    }

    public static String[] listChildren(String treeUri, String documentUri) {
        if (sResolver == null) {
            return new String[0];
        }
        try {
            Uri tree = Uri.parse(treeUri);
            String documentId = DocumentsContract.getDocumentId(
                Uri.parse(documentUri));
            Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(
                tree, documentId);
            Cursor cursor = sResolver.query(children,
                new String[]{DocumentsContract.Document.COLUMN_DOCUMENT_ID},
                null, null, null);
            if (cursor == null) {
                return new String[0];
            }
            ArrayList<String> result = new ArrayList<>();
            try {
                while (cursor.moveToNext()) {
                    result.add(DocumentsContract.buildDocumentUriUsingTree(
                        tree, cursor.getString(0)).toString());
                }
            } finally {
                cursor.close();
            }
            return result.toArray(new String[0]);
        } catch (Exception e) {
            return new String[0];
        }
    }

    public static String displayName(String documentUri) {
        return queryString(documentUri,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME);
    }

    public static String mimeType(String documentUri) {
        if (sResolver == null) {
            return null;
        }
        try {
            String mime = sResolver.getType(Uri.parse(documentUri));
            if (mime != null) {
                return mime;
            }
        } catch (Exception ignored) {
        }
        return queryString(documentUri,
            DocumentsContract.Document.COLUMN_MIME_TYPE);
    }

    private static String queryString(String documentUri, String column) {
        if (sResolver == null) {
            return null;
        }
        try {
            Cursor cursor = sResolver.query(Uri.parse(documentUri),
                new String[]{column}, null, null, null);
            if (cursor == null) {
                return null;
            }
            try {
                if (cursor.moveToFirst()) {
                    return cursor.getString(0);
                }
            } finally {
                cursor.close();
            }
        } catch (Exception ignored) {
        }
        return null;
    }

    public static String findChild(String treeUri, String parentDocumentUri,
                                   String displayName) {
        if (sResolver == null) {
            return null;
        }
        try {
            Uri tree = Uri.parse(treeUri);
            String parentId = DocumentsContract.getDocumentId(
                Uri.parse(parentDocumentUri));
            Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(
                tree, parentId);
            Cursor cursor = sResolver.query(children, new String[]{
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME
            }, null, null, null);
            if (cursor == null) {
                return null;
            }
            try {
                while (cursor.moveToNext()) {
                    if (displayName.equals(cursor.getString(1))) {
                        return DocumentsContract.buildDocumentUriUsingTree(
                            tree, cursor.getString(0)).toString();
                    }
                }
            } finally {
                cursor.close();
            }
        } catch (Exception ignored) {
        }
        return null;
    }

    public static String createFile(String treeUri, String parentDocumentUri,
                                    String displayName, String mimeType) {
        if (sResolver == null) {
            return null;
        }
        try {
            Uri created = DocumentsContract.createDocument(sResolver,
                Uri.parse(parentDocumentUri), mimeType, displayName);
            return created == null ? null : created.toString();
        } catch (Exception e) {
            return null;
        }
    }

    public static String renameDocument(String documentUri, String newName) {
        if (sResolver == null) {
            return null;
        }
        try {
            Uri renamed = DocumentsContract.renameDocument(sResolver,
                Uri.parse(documentUri), newName);
            return renamed == null ? null : renamed.toString();
        } catch (Exception e) {
            return null;
        }
    }

    public static boolean deleteDocument(String documentUri) {
        if (sResolver == null) {
            return false;
        }
        try {
            return DocumentsContract.deleteDocument(sResolver,
                Uri.parse(documentUri));
        } catch (Exception e) {
            return false;
        }
    }

    public static byte[] readBytes(String documentUri) {
        if (sResolver == null) {
            return null;
        }
        try (InputStream in = sResolver.openInputStream(
                 Uri.parse(documentUri))) {
            if (in == null) {
                return null;
            }
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buffer = new byte[8192];
            int count;
            while ((count = in.read(buffer)) > 0) {
                out.write(buffer, 0, count);
            }
            return out.toByteArray();
        } catch (Exception e) {
            return null;
        }
    }

    public static boolean writeBytes(String documentUri, byte[] data) {
        if (sResolver == null) {
            return false;
        }
        try (OutputStream out = sResolver.openOutputStream(
                 Uri.parse(documentUri), "wt")) {
            if (out == null) {
                return false;
            }
            out.write(data);
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    public static boolean exists(String documentUri) {
        if (sResolver == null) {
            return false;
        }
        try {
            Cursor cursor = sResolver.query(Uri.parse(documentUri),
                new String[]{DocumentsContract.Document.COLUMN_DOCUMENT_ID},
                null, null, null);
            if (cursor == null) {
                return false;
            }
            try {
                return cursor.moveToFirst();
            } finally {
                cursor.close();
            }
        } catch (Exception e) {
            return false;
        }
    }

    public static boolean isDirectory(String documentUri) {
        return DocumentsContract.Document.MIME_TYPE_DIR.equals(
            mimeType(documentUri));
    }
}
