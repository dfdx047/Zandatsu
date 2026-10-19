package com.dfdx047.swordman;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Typeface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;

/**
 * Launcher minimo (sem bibliotecas): mostra se o jogo esta no lugar, abre o jogo e
 * exporta os logs. O launcher completo (controles por toque, drivers, desempenho)
 * vem do Dragon Fury depois de o jogo arrancar.
 */
public class LauncherActivity extends Activity {
    private TextView mStatus;
    private Button mPlay;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        int pad = dp(24);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(pad, pad, pad, pad);
        root.setGravity(Gravity.CENTER_HORIZONTAL);

        TextView title = new TextView(this);
        title.setText(R.string.app_name);
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 28);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        root.addView(title);

        mStatus = new TextView(this);
        mStatus.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        mStatus.setPadding(0, dp(16), 0, dp(16));
        mStatus.setTextIsSelectable(true);
        root.addView(mStatus);

        mPlay = new Button(this);
        mPlay.setText(R.string.play);
        mPlay.setOnClickListener(v -> startActivity(new Intent(this, GameActivity.class)));
        root.addView(mPlay, wide());

        Button export = new Button(this);
        export.setText(R.string.export_logs);
        export.setOnClickListener(v -> exportLogs(export));
        root.addView(export, wide());

        ScrollView scroll = new ScrollView(this);
        scroll.addView(root);
        setContentView(scroll);
    }

    @Override
    protected void onResume() {
        super.onResume();
        refresh();
    }

    private void refresh() {
        File xex = AppPaths.xexFile(this);
        File media = AppPaths.mediaDir(this);
        boolean ok = xex.isFile() && media.isDirectory();
        StringBuilder sb = new StringBuilder();
        sb.append(getString(R.string.game_folder)).append('\n')
                .append(AppPaths.gameDir(this).getAbsolutePath()).append("\n\n");
        sb.append("default.xex: ").append(xex.isFile() ? "ok" : getString(R.string.missing)).append('\n');
        sb.append("media/: ").append(media.isDirectory() ? "ok" : getString(R.string.missing)).append('\n');
        if (!ok) {
            sb.append('\n').append(getString(R.string.push_hint));
        }
        mStatus.setText(sb);
        mPlay.setEnabled(ok);
    }

    private void exportLogs(Button button) {
        button.setEnabled(false);
        Handler main = new Handler(Looper.getMainLooper());
        new Thread(() -> {
            String msg;
            try {
                msg = getString(R.string.logs_saved, LogExporter.export(this));
            } catch (Exception e) {
                msg = getString(R.string.logs_failed, String.valueOf(e.getMessage()));
            }
            final String text = msg;
            main.post(() -> {
                button.setEnabled(true);
                Toast.makeText(this, text, Toast.LENGTH_LONG).show();
            });
        }, "export-logs").start();
    }

    private LinearLayout.LayoutParams wide() {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(8);
        return lp;
    }

    private int dp(int v) {
        return Math.round(v * getResources().getDisplayMetrics().density);
    }
}
