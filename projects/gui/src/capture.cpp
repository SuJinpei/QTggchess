#include "capture.h"

#include <QThread>
#include <QScreen>
#include <QPixmap>
#include <QRect>
#include <QGuiApplication>

#include <board/boardfactory.h>

enum ChinesePieceType
{
	eBPawn = 1,	//!< Pawn
	eBXiang,		//!< Knight
	eBShi,		//!< Bishop
	eBPao,		//!< Rook
	eBMa,		    //!< Ma
	eBChe,		//!< Queen
	eBKing,		//!< King

	eRPawn,	    //!< Pawn
	eRXiang,		//!< Knight
	eRShi,		//!< Bishop
	eRPao,		//!< Rook
	eRMa,		    //!< Ma
	eRChe,		//!< Queen
	eRKing		//!< King
};


Capture::Capture()
	: m_precision(0.96f),
	m_UseAdb(false),	
	m_sleepTimeMs(200),
	m_scaleX(1.0f),
	m_scaleY(1.0f),
	m_chessWinOK(false)
{	
	initBoard();
}

Capture::Capture(float precision, bool UseAdb, int sleepMs, float scX, float scY)
	: m_precision(precision),
	m_UseAdb(UseAdb),
	m_sleepTimeMs(sleepMs),
	m_scaleX(scX),
	m_scaleY(scY),
	m_chessWinOK(false)
	
{
	initBoard();
}

void Capture::initBoard()
{
	m_chessClip = 0.2f;
	m_LxInfo_isOK = false;
	this->m_board = Chess::BoardFactory::create("standard");
	//this->m_board_second = Chess::BoardFactory::create("standard");
	m_board->reset();
	//m_board_second->reset();
}



cv::Mat Capture::QImageToCvMat(const QImage& inImage, bool inCloneImageData)
{
	switch (inImage.format())
	{
		// 8-bit, 4 channel
	case QImage::Format_ARGB32:
	case QImage::Format_ARGB32_Premultiplied:
	{
		cv::Mat  mat(inImage.height(), inImage.width(),
			CV_8UC4,
			const_cast<uchar*>(inImage.bits()),
			static_cast<size_t>(inImage.bytesPerLine())
		);

		return (inCloneImageData ? mat.clone() : mat);
	}

	// 8-bit, 3 channel
	case QImage::Format_RGB32:
	case QImage::Format_RGB888:
	{
		if (!inCloneImageData)
		{
			qWarning() << "CVS::QImageToCvMat() - Conversion requires cloning because we use a temporary QImage";
		}

		QImage   swapped = inImage;

		if (inImage.format() == QImage::Format_RGB32)
		{
			swapped = swapped.convertToFormat(QImage::Format_RGB888);
		}

		swapped = swapped.rgbSwapped();

		return cv::Mat(swapped.height(), swapped.width(),
			CV_8UC3,
			const_cast<uchar*>(swapped.bits()),
			static_cast<size_t>(swapped.bytesPerLine())
		).clone();
	}

	// 8-bit, 1 channel
	case QImage::Format_Indexed8:
	{
		cv::Mat  mat(inImage.height(), inImage.width(),
			CV_8UC1,
			const_cast<uchar*>(inImage.bits()),
			static_cast<size_t>(inImage.bytesPerLine())
		);

		return (inCloneImageData ? mat.clone() : mat);
	}

	default:
		qWarning() << "CVS::QImageToCvMat() - QImage format not handled in switch:" << inImage.format();
		break;
	}

	return cv::Mat();	
	// https ://blog.csdn.net/qq_17550379/article/details/78683153
}

cv::Mat Capture::QPixmapToCvMat(const QPixmap& inPixmap, bool inCloneImageData)
{
	return QImageToCvMat(inPixmap.toImage(), inCloneImageData);
}



bool Capture::GetLxInfo(QString catlog)  // ֻҪ�鵽��û�г�������
{
	
	m_LxInfo_isOK = false;
	
	cLXinfo *pInfo = &this->m_LxInfo;

	stPieceList* pPieceList = &m_PieceList[0];

	pInfo->m_PieceCatlog = catlog;
	
	
	if (!getChessboardHwnd(true))
		return false;

	// 
	cv::Point Bche0;	
	cv::Point Bche1;

	if (pPieceList->BCheList[0].x < pPieceList->BCheList[1].x) {
		Bche0 = pPieceList->BCheList[0];
		Bche1 = pPieceList->BCheList[1];
	}
	else {
		Bche1 = pPieceList->BCheList[0];
		Bche0 = pPieceList->BCheList[1];
	}

	pInfo->offx = Bche0.x;
	pInfo->offy = Bche0.y;
	
	pInfo->m_dx = (Bche1.x - Bche0.x) / 8;
	pInfo->m_dy = pInfo->m_dx;

	if (pInfo->m_dx < 10) return false;
	
	pInfo->m_class = this->get_window_class(this->m_hwnd);
	pInfo->m_titleKeyword = this->get_window_title(this->m_hwnd);

	// �����̸�СһЩ����

	//
	m_LxInfo_isOK = true;

	return this->GetAllPiecePicture(pInfo);	

}


QChar Capture::Qpiece_to_char(int chess)
{
	static const  QChar PieceString[] 
		= { '0','p','b','a','c','n','r','k','P','B','A','C','N','R','K' };
	return PieceString[chess];
}

int Capture::getB90(cv::Point p)
{
	int dx = (int)((p.x - m_LxInfo.offx) / m_LxInfo.m_dx + 0.5f);
	int dy = (int)((p.y - m_LxInfo.offy) / m_LxInfo.m_dy + 0.5f);
	int s90 = dx + 9 * dy;

	if (s90 > 89) s90 = 89;
	return s90;
}

bool Capture::fillB90(int* b90, QVector<cv::Point> &plist, int chess)
{
	for (auto p : plist) {
		int s90 = getB90(p);
		b90[s90] = chess;
	}

	return true;
}

bool Capture::GetFen(stPieceList* pList)
{
	int b90[90] = { 0 };

	fillB90(b90, pList->RCheList,	 eRChe);
	fillB90(b90, pList->RMaList,	 eRMa);
	fillB90(b90, pList->RPaoList,	 eRPao);
	fillB90(b90, pList->RShiList,    eRShi);
	fillB90(b90, pList->RXiangList,  eRXiang);
	fillB90(b90, pList->RPawnList,   eRPawn);
	fillB90(b90, pList->RKingList,   eRKing);

	fillB90(b90, pList->BCheList,	 eBChe);
	fillB90(b90, pList->BMaList,	 eBMa);
	fillB90(b90, pList->BPaoList,	 eBPao);
	fillB90(b90, pList->BShiList,	 eBShi);
	fillB90(b90, pList->BXiangList,	 eBXiang);
	fillB90(b90, pList->BPawnList,	 eBPawn);
	fillB90(b90, pList->BKingList,	 eBKing);

	// getFen from B90

	bool flip = false;
	if (pList->BKingList[0].y > pList->RKingList[0].y) {
		flip = true;
	}
	
	QString fen;
	QChar c;
	
	for (int rank = 0; rank <= 9; rank++) {
		for (int file = 0; file <= 8; ) {
			int s90 = file + rank * 9;
			int chess = b90[s90];

			if (chess == 0) {
				int len = 0;
				for (; file <= 8 && b90[file+rank*9] == 0; file++) {
					len++;
				}
				c = '0' + len;
			}
			else {
				c = Qpiece_to_char(chess);
				file++;
			}
			fen += c;
		}
		fen += (rank < 9 ? '/' : ' ');
	}
	fen += (flip == false ? "w " : "b ");
	fen += "- - 0 1";

	pList->fen = fen;

	return true;
}

// �õ����е������б�
bool Capture::GetLxBoardChess(bool org)
{
	
	if (this->m_LxInfo_isOK == false)
		this->GetLxInfo("0");

	if (this->m_LxInfo_isOK == false) return false;

	// 
	stPieceList* pList = &m_PieceList[1];
	if (org) {
		pList = &m_PieceList[0];
	}

	if (!SearchOnChessList(m_hwnd, "bk.png", pList->BKingList, true)) {    // �ڽ�
		return false;  // �Ҳ����Է��Ľ���
	}
	
	SearchOnChessList(m_hwnd, "br.png", pList->BCheList);     // �ڳ�
	SearchOnChessList(m_hwnd, "bn.png", pList->BMaList);      // ����
	SearchOnChessList(m_hwnd, "bc.png", pList->BPaoList);     // ����
	SearchOnChessList(m_hwnd, "ba.png", pList->BShiList);     // ��ʿ
	SearchOnChessList(m_hwnd, "bb.png", pList->BXiangList);   // ����
	SearchOnChessList(m_hwnd, "bp.png", pList->BPawnList);    // �ڱ�
	
	
	SearchOnChessList(m_hwnd, "rr.png", pList->RCheList);     // �쳵
	SearchOnChessList(m_hwnd, "rn.png", pList->RMaList);      // ����
	SearchOnChessList(m_hwnd, "rc.png", pList->RPaoList);     // ����
	SearchOnChessList(m_hwnd, "ra.png", pList->RShiList);     // ��ʿ
	SearchOnChessList(m_hwnd, "rb.png", pList->RXiangList);   // ����
	SearchOnChessList(m_hwnd, "rp.png", pList->RPawnList);    // ���
	SearchOnChessList(m_hwnd, "rk.png", pList->RKingList);    // �콫
	
	return GetFen(pList);
}

// �õ����е����ӵ�ͼƬ
bool Capture::GetAllPiecePicture(cLXinfo* pInfo)
{
	

	// Bche
	SaveOnePiecePic(pInfo, 0, 0, "br.png");
	SaveOnePiecePic(pInfo, 1, 0, "bn.png");
	SaveOnePiecePic(pInfo, 2, 0, "bb.png");
	SaveOnePiecePic(pInfo, 3, 0, "ba.png");
	SaveOnePiecePic(pInfo, 4, 0, "bk.png");

	SaveOnePiecePic(pInfo, 1, 2, "bc.png");
	SaveOnePiecePic(pInfo, 0, 3, "bp.png");

	// �쳵
	SaveOnePiecePic(pInfo, 0, 9, "rr.png");
	SaveOnePiecePic(pInfo, 1, 9, "rn.png");
	SaveOnePiecePic(pInfo, 2, 9, "rb.png");
	SaveOnePiecePic(pInfo, 3, 9, "ra.png");
	SaveOnePiecePic(pInfo, 4, 9, "rk.png");

	SaveOnePiecePic(pInfo, 1, 7, "rc.png");  // ����
	SaveOnePiecePic(pInfo, 0, 6, "rp.png");
	
	return true;
}

bool Capture::SaveOnePiecePic(cLXinfo* pInfo, int x, int y, QString chessName)
{
	int pieceSize = int(pInfo->m_dx * 0.65f);
	QString picPath = getFindPath() + pInfo->m_PieceCatlog + "/" + chessName;

	QRect rect(
		pInfo->offx - pieceSize / 2 + x * pInfo->m_dx,
		pInfo->offy - pieceSize / 2 + y * pInfo->m_dx,
		pieceSize, pieceSize);
	QPixmap cropped = this->m_capPixmap.copy(rect);
	cropped.save(picPath, "PNG");   // �ڳ�

	return true;
}

Capture::~Capture()
{
	delete this->m_board;
	//delete this->m_board_second;
}

bool Capture::captureOne(QString fname, HWND hw, bool disp, int sleepTimeMs, QString path)
{
	Q_UNUSED(disp);

	if (path == nullptr) {
		path = this->getPicturePath();
	}
	if (this->m_UseAdb) {
		//return this->pAdb->one_screenshot(this->capPixmap, disp, fname, sleepTimeMs, path);
		return false;
	}
	else {
		return this->CaptureOneNotry(fname, hw, sleepTimeMs, path);
	}
}

bool Capture::searchImage(QString findName, bool isCap, QString mainName, HWND hw)
{
	QVector<cv::Point> res;
	if (hw == nullptr) {
		hw = this->m_hwnd;
	}
	return this->searchImage(hw, findName, res, mainName, isCap);
}

bool Capture::searchImage(HWND hw, QString findName, QVector<cv::Point> &res, QString mainName, bool isCap, float threshold, bool isShow)
{
	if (isCap) {
		if (this->captureOne(mainName, hw) == false) {
			qWarning("searchImage 1 %s �����ˣ�", findName);
			return false;
		}
	}

	cv::Mat image_template_scaled;
	//cv::Mat image_source;

	try {
		if (mainName == nullptr) {
			
			//cv::Mat mat = QImage_to_cvMat(this->capPixmap.toImage(), false);
			//image_source = cv::Mat(mat.rows, mat.cols, CV_8UC3);
			//int from_to[] = { 0,0,  1,1,  2,2 };
			//cv::mixChannels(&mat, 1, &image_source, 1, from_to, 3);

			//image_source = mat2;
		}
		else {
			QString mFile = this->getPicturePath() + mainName;
			m_image_source = cv::imread(mFile.toStdString()); // ��ͼ
		}

		//QString hashName = "aff"; // getHashName(findName);

		if (this->m_MatHash.contains(findName)) {              // �����ڻ�����
			image_template_scaled = this->m_MatHash.value(findName);
		}
		else {
			QString fFile = this->getFindPath() + findName;
			cv::Mat image_template = cv::imread(fFile.toStdString());   // ģ��ͼ

			
			cv::resize(image_template, image_template_scaled, cv::Size(), this->m_scaleX, this->m_scaleY);
			

			// ������ٲ�һ�£���Ϊ�б߿����		

			int w = image_template_scaled.rows;
			int h = image_template_scaled.cols;
			float s = this->m_chessClip;
			cv::Rect crect(w * s, h * s, w * (1 - 2 * s), h * (1 - 2 * s));
			image_template_scaled = image_template_scaled(crect);
		
			this->m_MatHash.insert(findName, image_template_scaled);
		}


		//QString fFile = this->getFindPath() + findName;
		//cv::Mat image_template = cv::imread(fFile.toStdString());   // ģ��ͼ
		//cv::resize(image_template, image_template_scaled, cv::Size(), this->scaleX, this->scaleY);

	}
	catch (...) {
		qWarning("searchImage 2 %s �����ˣ�", findName);
		return false;
	}


	//cv::imshow("img", image_source);
	//cv::imshow("templ", image_template_scaled);
	//cv::waitKey();

	cv::Mat image_matched;
	try {
		cv::matchTemplate(m_image_source, image_template_scaled, image_matched, cv::TM_CCOEFF_NORMED);
	}
	catch (...) {
		qWarning("searchImage 3 %s �����ˣ�", findName);
		return false;
	}

	//cv::imshow("templ", image_template_scaled);
	//cv::imshow("img", image_source);
	//cv::imshow("matched", image_matched);
	//cv::GOB::WaitKey();

	if (threshold == 1.0f) {
		threshold = this->m_precision;
	}
	res.clear();  // �������

	bool Isfind = false;

	while (true) {
		cv::Point minLoc, maxLoc;
		double minVal, maxVal;

		//Ѱ�����ƥ��λ��
		cv::minMaxLoc(image_matched, &minVal, &maxVal, &minLoc, &maxLoc);

		if (maxVal > threshold) {

			Isfind = true;

			cv::Point chessP;

			chessP.x = maxLoc.x + image_template_scaled.cols / 2;
			chessP.y = maxLoc.y + image_template_scaled.rows / 2;

			if (isShow) {
				//cv::Mat image_color;
				//cv::cvtColor(image_source, image_color, cv::CV_BGR2GRAY);
				cv::circle(m_image_source,
					chessP, //cv::Point(imgX, imgY),
					image_template_scaled.rows,
					cv::Scalar(0, 255, 255),
					2,
					8,
					0);

				cv::imshow("target", m_image_source);
				cv::imshow("templ", image_template_scaled);
			}

			res.append(chessP);

			//cv::imshow("m1", image_matched);
			cv::floodFill(image_matched, maxLoc, cv::Scalar(0));		
			//cv::imshow("m2", image_matched);
		}
		else {
			break;
		}

	}

	return Isfind;
}

cv::Mat Capture::QImage_to_cvMat(const QImage& image, bool inCloneImageData)
{
	switch (image.format())
	{
		// ��ӦMat��8λ4ͨ��ͼ��
	case QImage::Format_RGB32:
	{
		cv::Mat mat(image.height(), image.width(), CV_8UC4, const_cast<uchar*>(image.bits()), image.bytesPerLine());
		return (inCloneImageData ? mat.clone() : mat);
	}

	// ��ӦMat��8λ3ͨ��ͼ��
	case QImage::Format_RGB888:
	{
		if (!inCloneImageData) {
			//qWarning() << "ASM::QImageToCvMat() - Conversion requires cloning since we use a temporary QImage";
		}

		//rgbSwapped�������ڷ�תRBͨ����������һ����QImage��������ı�ԭ��ͼ��
		QImage swapped = image.rgbSwapped();
		return cv::Mat(swapped.height(), swapped.width(), CV_8UC3, const_cast<uchar*>(swapped.bits()), swapped.bytesPerLine()).clone();
	}

	// 8λ��ͨ��
	case QImage::Format_Indexed8:
	{
		cv::Mat  mat(image.height(), image.width(), CV_8UC1, const_cast<uchar*>(image.bits()), image.bytesPerLine());

		return (inCloneImageData ? mat.clone() : mat);
	}

	default:

		//qDebug() << "Image format is not supported: depth=%d and %d format\n", image.depth(), image.format();
		break;
	}

	return cv::Mat();

	// https ://blog.csdn.net/wingWC/article/details/78724435 

}

bool Capture::CaptureOneNotry(QString fname, HWND hw, int sleepTimeMS, QString path)
{
	
	try {

		QThread::msleep(sleepTimeMS);
	
		QScreen* screen = QGuiApplication::primaryScreen();

		this->m_capPixmap = screen->grabWindow(WId(hw));

		//fname = "tmp2.png";
		if (fname != nullptr) {
			this->m_capPixmap.save(path + fname, "PNG");
		}

		//QString cname = this->get_window_class(hw);

		// ת���� openCV ��ʽ
		/*
		cv::Mat mat = QImage_to_cvMat(this->m_capPixmap.toImage(), false);
		m_image_source = cv::Mat(mat.rows, mat.cols, CV_8UC3);
		int from_to[] = { 0,0,  1,1,  2,2 };
		cv::mixChannels(&mat, 1, &m_image_source, 1, from_to, 3);
		*/

		this->m_image_source = QPixmapToCvMat(this->m_capPixmap, true);

		if (this->m_LxInfo_isOK) {  // ��ǰ��������ϢOK��

			//this->m_LxInfo.m_dx

			cv::Rect crect(0, 0, m_LxInfo.offx + m_LxInfo.m_dx*8.8, m_LxInfo.offy+ m_LxInfo.m_dx * 9.8);
			m_image_source = m_image_source(crect);

			//cv::imshow("templ", m_image_source);
			//cv::waitKey();
		}

		// isDisplay	
		//if (this->displayCapture) {
		//	GOB::DisplayImage(this->capPixmap, this->isZoom);
		//}

	}
	catch (...) {
		qWarning("��ͼ�����ˣ� %s", path + fname);
		return false;
	}
	return  true;
}

bool Capture::getChessboardHwnd(bool onlyBChe)
{	
	HWND hw = first_window();
	while (hw != nullptr)
	{
		if (isChessBoardWindow(hw, &m_PieceList[0], onlyBChe)) {
			//if (isMoveOrg) {
			//	RECT  rect;
			//	GetClientRect(hw, &rect);
			//	::MoveWindow(hw, 0, 0, rect.right, rect.top, false);
			//}
			this->m_hwnd = hw;
			return true;
		}
		hw = next_window(hw);
	}	
	return false;
}

bool Capture::check_window_valid(HWND window, int minW, int minH, window_search_mode mode)
{
	DWORD styles, ex_styles;
	RECT  rect;

	if (/*!IsWindowVisible(window) ||*/
		(mode == EXCLUDE_MINIMIZED && IsIconic(window)))
		return false;

	GetClientRect(window, &rect);
	styles = DWORD(GetWindowLongPtr(window, GWL_STYLE));
	ex_styles = DWORD(GetWindowLongPtr(window, GWL_EXSTYLE));

	if (ex_styles & WS_EX_TOOLWINDOW)
		return false;
	if (styles & WS_CHILD)
		return false;
	//if (mode == EXCLUDE_MINIMIZED && (rect.bottom == 0 || rect.right == 0))
	//if (mode == EXCLUDE_MINIMIZED && (rect.bottom <= minH || rect.right <= minW))

	if (rect.bottom <= minH || rect.right <= minW)
		return false;

	return true;
}

inline HWND Capture::next_window(HWND window, window_search_mode mode)
{
	while (true) {
		window = GetNextWindow(window, GW_HWNDNEXT);
		if (!window || check_window_valid(window, validWinW, validWinH, mode))
			break;
	}
	return window;
}

inline HWND Capture::first_window(window_search_mode mode)
{
	HWND window = GetWindow(GetDesktopWindow(), GW_CHILD);
	if (!check_window_valid(window, validWinW, validWinH, mode))
		window = next_window(window, mode);
	return window;
}

QString Capture::get_window_title(HWND hwnd)
{
	QString retStr;
	wchar_t temp[512];
	int len;

	len = GetWindowTextLengthW(hwnd);
	if (!len)
		return retStr;

	//temp = reinterpret_cast<wchar_t *>(malloc(sizeof(wchar_t) * (quint32(len)+1)));
	if (GetWindowTextW(hwnd, temp, len + 1))
	{
		retStr = QString::fromWCharArray(temp);
	}
	//free(temp);
	return retStr;
}

QString Capture::get_window_class(HWND hwnd)
{
	QString retStr;
	wchar_t temp[256];

	temp[0] = 0;
	if (GetClassNameW(hwnd, temp, sizeof(temp) / sizeof(wchar_t)))
	{
		retStr = QString::fromWCharArray(temp);
	}
	return retStr;
}

bool Capture::SearchOnChessList(HWND hwnd, QString chess, QVector<cv::Point>& res, bool isCap)
{
	QString chessFile = m_LxInfo.m_PieceCatlog + "/" + chess;  // �ڳ�
	
	return searchImage(hwnd, chessFile, res, nullptr, isCap);
}

bool Capture::isChessBoardWindow(HWND hwnd, stPieceList* pieceList, bool onlyBche)
{
	// +cname	Qt5QWindowIcon	QString
	QString wc = this->get_window_class(hwnd);
	if (wc == "Qt5QWindowIcon") {   // ȥ���Լ��Ĵ���
		return false;
	}

	if (onlyBche) {
		//QString rk = m_LxInfo.m_PieceCatlog + "/br.png";  // �ڳ�
		//if (!searchImage(hwnd, rk, pieceList->BCheList, nullptr, true)) {

		if(!SearchOnChessList(hwnd,"br.png", pieceList->BCheList, true)){
			return false;
		}
		if (pieceList->BCheList.count() == 2) {
			return true;
		}
	}
	else {  // ���ж�˫���н�

		if (!SearchOnChessList(hwnd, "bk.png", pieceList->BKingList, true)) {
			return false;
		}

		if (!SearchOnChessList(hwnd, "rk.png", pieceList->RKingList)) {
			return false;
		}

		if (pieceList->RKingList.count() >= 1 && pieceList->BKingList.count() >= 1) {
			return true;  // ���Ӱѽ���ʱ�䷽�ˣ���
		}
	}

	return false;
}

QString Capture::getPicturePath()
{
	QString runPath = QCoreApplication::applicationDirPath();
	QString str = runPath + "/image/";	
	return str;
}

QString Capture::getFindPath()
{
	QString runPath = QCoreApplication::applicationDirPath();
	QString str = runPath + "/image/findchess/";
	return str;
}



//QString Capture::getHashName(QString orgStr)
//{
//	QStringList stFList = orgStr.split("/");		// by LGL
//	QString ret;
//	for (auto st : stFList) {
//		ret += st;
//	}	
//	return ret;
//}

//Capture::cLXinfo::cLXinfo()
//{
//	//chessClip = 0.2f;
//}
//
//Capture::cLXinfo::~cLXinfo()
//{
//}
