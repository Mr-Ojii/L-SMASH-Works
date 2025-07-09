//----------------------------------------------------------------------------------
//	入力プラグイン ヘッダーファイル for AviUtl ExEdit2
//	By ＫＥＮくん
//  Modified for C99 by Mr-Ojii on 2025-07-08
//----------------------------------------------------------------------------------

static const int FLAG_VIDEO = 1;
static const int FLAG_AUDIO = 2;
static const int FLAG_CONCURRENT = 16;

// 入力ファイル情報構造体
// 画像フォーマットはRGB24bit,RGBA32bit,YUY2が対応しています
// 音声フォーマットはPCM16bit,PCM(float)32bitが対応しています
typedef struct INPUT_INFO {
    int	flag;					// フラグ
    // static constexpr int FLAG_VIDEO = 1; // 画像データあり
    // static constexpr int FLAG_AUDIO = 2; // 音声データあり
    // static constexpr int FLAG_CONCURRENT = 16; // 画像・音声データの同時取得をサポートする ※読み込み関数が同時に呼ばれる
    int	rate, scale;			// フレームレート、スケール
    int	n;						// フレーム数
    BITMAPINFOHEADER* format;	// 画像フォーマットへのポインタ(次に関数が呼ばれるまで内容を有効にしておく)
    int	format_size;			// 画像フォーマットのサイズ
    int audio_n;				// 音声サンプル数
    WAVEFORMATEX* audio_format;	// 音声フォーマットへのポインタ(次に関数が呼ばれるまで内容を有効にしておく)
    int audio_format_size;		// 音声フォーマットのサイズ
} INPUT_INFO;

// 入力ファイルハンドル
typedef void* INPUT_HANDLE;

// 入力プラグイン構造体
typedef struct INPUT_PLUGIN_TABLE {
    int flag;					// フラグ
    // static constexpr int FLAG_VIDEO = 1; //	画像をサポートする
    // static constexpr int FLAG_AUDIO = 2; //	音声をサポートする
    LPCWSTR name;				// プラグインの名前
    LPCWSTR filefilter;			// 入力ファイルフィルタ
    LPCWSTR information;		// プラグインの情報

    // 入力ファイルをオープンする関数へのポインタ
    // file		: ファイル名
    // 戻り値	: TRUEなら入力ファイルハンドル
    INPUT_HANDLE (*func_open)(LPCWSTR file);

    // 入力ファイルをクローズする関数へのポインタ
    // ih		: 入力ファイルハンドル
    // 戻り値	: TRUEなら成功
    bool (*func_close)(INPUT_HANDLE ih);

    // 入力ファイルの情報を取得する関数へのポインタ
    // ih		: 入力ファイルハンドル
    // iip		: 入力ファイル情報構造体へのポインタ
    // 戻り値	: TRUEなら成功
    bool (*func_info_get)(INPUT_HANDLE ih, INPUT_INFO* iip);

    // 画像データを読み込む関数へのポインタ
    // ih		: 入力ファイルハンドル
    // frame	: 読み込むフレーム番号
    // buf		: データを読み込むバッファへのポインタ
    // 戻り値	: 読み込んだデータサイズ
    int (*func_read_video)(INPUT_HANDLE ih, int frame, void* buf);

    // 音声データを読み込む関数へのポインタ
    // ih		: 入力ファイルハンドル
    // start	: 読み込み開始サンプル番号
    // length	: 読み込むサンプル数
    // buf		: データを読み込むバッファへのポインタ
    // 戻り値	: 読み込んだサンプル数
    int (*func_read_audio)(INPUT_HANDLE ih, int start, int length, void* buf);

    // 入力設定のダイアログを要求された時に呼ばれる関数へのポインタ (nullptrなら呼ばれません)
    // hwnd			: ウィンドウハンドル
    // dll_hinst	: インスタンスハンドル
    // 戻り値		: TRUEなら成功
    bool (*func_config)(HWND hwnd, HINSTANCE dll_hinst);
} INPUT_PLUGIN_TABLE;
