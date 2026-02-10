//----------------------------------------------------------------------------------
//	ログ出力機能 ヘッダーファイル for AviUtl ExEdit2
//	By ＫＥＮくん
//	Modified for C99 by Mr-Ojii on 2026-02-21
//----------------------------------------------------------------------------------

//	各種プラグインで下記の関数を外部公開すると呼び出されます
// 
//	ログ出力機能初期化関数
//		void InitializeLogger(LOG_HANDLE* logger)
//		※InitializePlugin()より先に呼ばれます

//----------------------------------------------------------------------------------


// ログ出力ハンドル
// ログ出力は1024文字で制限されます
typedef struct LOG_HANDLE {
	// プラグイン用のログを出力します
	// handle	: ログ出力ハンドル
	// message	: ログメッセージ
	void (*log)(struct LOG_HANDLE* handle, LPCWSTR message);

	// infoレベルのログを出力します
	// handle	: ログ出力ハンドル
	// message	: ログメッセージ
	void (*info)(struct LOG_HANDLE* handle, LPCWSTR message);

	// warnレベルのログを出力します
	// handle	: ログ出力ハンドル
	// message	: ログメッセージ
	void (*warn)(struct LOG_HANDLE* handle, LPCWSTR message);

	// errorレベルのログを出力します
	// handle	: ログ出力ハンドル
	// message	: ログメッセージ
	void (*error)(struct LOG_HANDLE* handle, LPCWSTR message);

	// verboseレベルのログを出力します
	// handle	: ログ出力ハンドル
	// message	: ログメッセージ
	void (*verbose)(struct LOG_HANDLE* handle, LPCWSTR message);

} LOG_HANDLE;
