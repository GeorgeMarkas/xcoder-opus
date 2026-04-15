package io.github.georgemarkas.xcoder;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

public class NativeLoader {

    public static void loadLibrary(String libName) throws IOException {
        if (!"linux".equalsIgnoreCase(System.getProperty("os.name")) ||
                !"amd64".equalsIgnoreCase(System.getProperty("os.arch"))) {
            throw new UnsupportedOperationException("Only Linux x86-64 is supported");
        }

        final String platformLibName = System.mapLibraryName(libName);
        final String libPath = "/native/linux-x86-64/" + platformLibName;

        try (InputStream inputStream = NativeLoader.class.getResourceAsStream(libPath)) {
            if (inputStream == null) {
                throw new RuntimeException("Could not find '" + platformLibName + "' at '" + libPath + "'");
            }

            // Extract to a temporary file that will be deleted on JVM exit
            Path temp = Files.createTempFile("native-" + libName, null);
            temp.toFile().deleteOnExit();
            Files.copy(inputStream, temp, StandardCopyOption.REPLACE_EXISTING);

            // Load the native library from the temporary file
            System.load(temp.toAbsolutePath().toString());
        }
    }
}
