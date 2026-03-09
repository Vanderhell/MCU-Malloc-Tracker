#include "mtv1_commands.h"
#include "mtv1_util.h"
#include <stdio.h>
#include <string.h>

void print_usage(void) {
    printf("MTV1 Stream Viewer - MCU Malloc Tracker\n\n");
    printf("Usage: mt_viewer_c <command> [options]\n\n");
    printf("Commands:\n");
    printf("  capture     Capture MTV1 stream from serial port\n");
    printf("  replay      Replay and visualize captured stream\n");
    printf("  diff        Compare two captured streams\n");
    printf("  export      Export stream to CSV/JSONL\n");
    printf("\nCapture Options:\n");
    printf("  --port <PORT>      COM port or device (required)\n");
    printf("  --baud <BAUD>      Baud rate (default: 921600)\n");
    printf("  --out <FILE>       Output file (required)\n");
    printf("  --seconds <N>      Capture for N seconds (0=until END)\n");
    printf("\nReplay Options:\n");
    printf("  --in <FILE>        Input MTV1 stream (required)\n");
    printf("  --filemap <FILE>   Filemap for file_id resolution\n");
    printf("  --no-tui           Text-only output (skip TUI)\n");
    printf("\nDiff Options:\n");
    printf("  --a <FILE>         First stream (required)\n");
    printf("  --b <FILE>         Second stream (required)\n");
    printf("  --out <FILE>       Output report.md (required)\n");
    printf("  --filemap <FILE>   Filemap for file_id resolution\n");
    printf("\nExport Options:\n");
    printf("  --in <FILE>        Input MTV1 stream (required)\n");
    printf("  --csv <FILE>       Output CSV file\n");
    printf("  --jsonl <FILE>     Output JSONL file\n");
}

int cmd_capture(int argc, char** argv) {
    mtv1_args_t args;
    mtv1_args_init(&args, argc, argv);

    mtv1_capture_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.port = mtv1_args_get(&args, "--port");
    opts.baud = 921600;  /* default */
    const char* baud_str = mtv1_args_get(&args, "--baud");
    if (baud_str)
        opts.baud = (uint32_t)atoi(baud_str);
    opts.out_path = mtv1_args_get(&args, "--out");
    opts.seconds = 0;
    const char* seconds_str = mtv1_args_get(&args, "--seconds");
    if (seconds_str)
        opts.seconds = atoi(seconds_str);

    return mtv1_capture_run(&opts);
}

int cmd_replay(int argc, char** argv) {
    mtv1_args_t args;
    mtv1_args_init(&args, argc, argv);

    mtv1_replay_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.in_path = mtv1_args_get(&args, "--in");
    opts.filemap_path = mtv1_args_get(&args, "--filemap");
    opts.no_tui = mtv1_args_has(&args, "--no-tui");

    return mtv1_replay_run(&opts);
}

int cmd_diff(int argc, char** argv) {
    mtv1_args_t args;
    mtv1_args_init(&args, argc, argv);

    mtv1_diff_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.path_a = mtv1_args_get(&args, "--a");
    opts.path_b = mtv1_args_get(&args, "--b");
    opts.out_path = mtv1_args_get(&args, "--out");
    opts.filemap_path = mtv1_args_get(&args, "--filemap");

    return mtv1_diff_run(&opts);
}

int cmd_export(int argc, char** argv) {
    mtv1_args_t args;
    mtv1_args_init(&args, argc, argv);

    mtv1_export_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.in_path = mtv1_args_get(&args, "--in");
    opts.csv_path = mtv1_args_get(&args, "--csv");
    opts.jsonl_path = mtv1_args_get(&args, "--jsonl");
    opts.filemap_path = mtv1_args_get(&args, "--filemap");

    return mtv1_export_run(&opts);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "capture") == 0) {
        return cmd_capture(argc - 1, argv + 1);
    } else if (strcmp(cmd, "replay") == 0) {
        return cmd_replay(argc - 1, argv + 1);
    } else if (strcmp(cmd, "diff") == 0) {
        return cmd_diff(argc - 1, argv + 1);
    } else if (strcmp(cmd, "export") == 0) {
        return cmd_export(argc - 1, argv + 1);
    } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
        print_usage();
        return 0;
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage();
        return 1;
    }
}
