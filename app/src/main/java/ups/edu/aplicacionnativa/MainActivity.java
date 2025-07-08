package ups.edu.aplicacionnativa;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.graphics.Bitmap;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.Toast;

import ups.edu.aplicacionnativa.databinding.ActivityMainBinding;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("aplicacionnativa");
    }

    private ActivityMainBinding binding;

    public native Bitmap processAndReturnBitmap(Bitmap bitmap);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        // Botón Procesar
        binding.buttonProcess.setOnClickListener(v -> {
            // Mejor usar bitmap redimensionado para mantener consistencia en procesamiento
            Bitmap bitmap = binding.drawingView.getBitmapResized(256, 256);

            if (bitmap != null) {
                Bitmap processed = processAndReturnBitmap(bitmap);
                if (processed != null) {
                    binding.imageViewProcessed.setImageBitmap(processed);
                } else {
                    Toast.makeText(this, "Error procesando imagen", Toast.LENGTH_SHORT).show();
                }
            } else {
                Toast.makeText(this, "No hay dibujo", Toast.LENGTH_SHORT).show();
            }
        });

        // Botón Limpiar
        binding.buttonClear.setOnClickListener(v -> {
            binding.drawingView.clear();               // Limpia el canvas de dibujo
            binding.imageViewProcessed.setImageBitmap(null); // Limpia la imagen procesada mostrada
        });
    }

}
