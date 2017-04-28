#include "calibgraph.hpp"

const String CalibGraph::Report::s_outDocFileName = "index.html";
const String CalibGraph::Report::s_outImgDirName  = "img";
const String CalibGraph::Report::s_outPlotDirName = "plot";

/**
 * A gnuplot helper class.
 */
class Plot
{
public:
	/* ctor */ Plot(const Path& plotPath, const String& plotName, const cv::Size& plotSize, const String& comment = "");
	/* dtor */ virtual	~Plot() {};
	bool Render();

	std::ofstream data;
	std::ofstream script;

	inline bool IsOkay() const { return data.is_open() && script.is_open(); }

	static Path gnuplotBinPath;

private:
	const String m_plotName;
	String m_command;
};

Path Plot::gnuplotBinPath = Path("gnuplot");

Plot::Plot(const Path& plotPath, const String& plotName, const cv::Size& plotSize, const String& comment)
: m_plotName(plotName)
{
	const Path dataPath   = plotPath / (plotName + ".dat"); // <path_to_plot>/<plot_name>.dat
	const Path scriptPath = plotPath / (plotName + ".gp" ); // <path_to_plot>/<plot_name>.gp
	const Path renderPath = plotPath / (plotName + ".png"); // <path_to_plot>/<plot_name>.png

	// build the command to be invoked by the OS to render the plot:
	// <path_to_gnuplot_bin> -e "cd '<path_to_plot>'" <script_filename>
    // e.g. /usr/bin/gniplot -e "cd '/home/johnny/cal/errorPlot'" errors.gp
	std::stringstream ss;
	ss << gnuplotBinPath.string() << " -e \"cd '" << plotPath << "'\" " << scriptPath.filename();
	m_command = ss.str();

	data.open(dataPath.string());
	if (!data.is_open())
	{
		E_ERROR << "error opening gnuplot data stream " << dataPath;
		return;
	}

	script.open(scriptPath.string());
	if (!script.is_open())
	{
		E_ERROR << "error opening gnuplot script stream " << scriptPath;
		return;
	}

	const Time now = unow();
	const String dateTimeString = time2string(now);

	// the data stream will be used by the script
	data << "# GNUPLOT data file for " << plotName << std::endl;
	data << "# Generated by calibn on " << dateTimeString << std::endl;

	// the script to be executed by gnuplot to render the plot and save the rendering to a PNG file
	script << "# GNUPLOT script file for " << plotName << std::endl;
	script << "# " << comment << std::endl;
	script << "# Generated by calibn on " << dateTimeString << std::endl;
	script << "data = \"" << dataPath.filename() << "\"" << std::endl;
	script << "set terminal png enhanced font \",9\" size " << plotSize.width << "," << plotSize.height << std::endl;
	script << "set output \"" << renderPath.string() << "\"" << std::endl;
}

bool Plot::Render()
{
	if (!IsOkay()) return false;

	data.flush();
	script.flush();

	int retval = system(m_command.c_str());

	if (retval != 0)
	{
		E_ERROR << "error rendering plot " << m_plotName;
		E_ERROR << "failed command: " << m_command;
		E_ERROR << "system returns " << retval;

		return false;
	}

	return true;
}

void CalibGraph::Report::SetGnuplotPath(const Path& path) const
{
    Plot::gnuplotBinPath = path;
}

bool CalibGraph::Report::Create(const Path& to)
{
	Clear();

	// m_fontFace(FONT_HERSHEY_PLAIN), m_fontScale(1.0),
	// m_frontColour(Scalar(0,255,0)), m_backColour(Scalar(0,0,0)), m_colour1(Scalar(255,0,0)), m_colour2(Scalar(0,0,255)), m_markerSize(7)
	const Path imgPath  = to / s_outImgDirName;
	const Path plotPath = to / s_outPlotDirName;
	const Path docPath  = to / s_outDocFileName;

	if (!makeOutDir(imgPath))
	{
		E_ERROR << "error creating image directory " << imgPath;
		return false;
	}

	if (!makeOutDir(plotPath))
	{
		E_ERROR << "error creating plot directory " << plotPath;
		return false;
	}

	m_stream.open(docPath.string());

	if (!m_stream.is_open())
	{
		E_ERROR << "error creating output file stream to " << docPath;
		return false;
	}

	WriteHeader();

	return true;
}

void CalibGraph::Report::Clear()
{
	if (!IsOkay()) return;

	WriteFooter();

	m_stream.flush();
	m_stream.close();
}

void CalibGraph::Report::WriteHeader()
{
	m_stream <<
	"<html>" << std::endl <<
	"<head>" << std::endl <<
	"	<title>Camera Calibration Report</title>" << std::endl <<
	"	<style type=\"text/css\">" << std::endl <<
	"		body		{color: #111; background-color: #fff}" << std::endl <<
	"		.footer		{text-align: center; font-size: 10pt; color: #ccc}" << std::endl <<
	"		.params	td	{background-color: #eee; color: #111; font-family: \"Courier New\", Courier, monospace; font-size: 10pt}" << std::endl <<
	"		td.marked	{background-color: #eee; color: #000; font-weight: bold}" << std::endl <<
	"		td.disabled	{background-color: #333; color: #ccc}" << std::endl <<
	"		div.block	{float: left; margin: 16px; padding: 16px; background-color: #ddd; border: solid 2px #777; border-radius: 12px}" << std::endl <<
	"		div.rpe		{float: left}" << std::endl <<
	"		form		{font-family: sans-serif; border: none; background: #333; padding: 10px}" << std::endl <<
	"		div.visCtrl	{display: inline-block; color: #fff;  margin: 2px; pedding: 3px;}" << std::endl <<
	"		div.visCtrl input[type=\"checkbox\"]{display: none}" << std::endl <<
	"		div.visCtrl input[type=\"checkbox\"] + label{display: block; padding: .4em .8em; line-height: 1; color: #eee; background-color: #444}" << std::endl <<
	"		div.visCtrl input[type=\"checkbox\"] + label:hover{background-color: #ccc; color:  #555}" << std::endl <<
	"		div.visCtrl input[type=\"checkbox\"]:checked + label {background-color: #eee; color: #333}" << std::endl <<
	"		div.visCtrl input[type=\"checkbox\"]:checked + label:hover {background-color: #ccc; color: #555}" << std::endl <<
	"		div.visCtrl input[type=\"checkbox\"]:disabled + label {background-color: #000; color: #333}" << std::endl <<
	"	</style>" << std::endl <<
	"	<script language=\"javascript\">" << std::endl <<
	"		function toggleImg(imgId, chkId) {" << std::endl <<
	"			var chk = document.getElementById(chkId);" << std::endl <<
	"			var img = document.getElementById(imgId);" << std::endl <<
	"			img.style.display = chk.checked ? \"none\" : \"block\";" << std::endl <<
	"		}" << std::endl <<
	"	</script>" << std::endl <<
	"</head>" << std::endl <<
	"<body>" << std::endl <<
	"	<h1>Multiocular Camera Calibration Report</h1>" << std::endl <<
	"	<span>Generation Time: " << time2string(unow()) << "</span>" << std::endl <<
	"	<br clear=\"all\" />" << std::endl;
}

void CalibGraph::Report::WriteFooter()
{
	m_stream <<
	"	<br clear=\"all\" />" << std::endl <<
	"	<hr />" << std::endl <<
	"	<span class=\"footer\">Report generated by <em>calibn</em></span>" << std::endl <<
	"</body>" << std::endl <<
	"</html>" << std::endl;
}
/*
void CalibReport::WriteVisibilityGraph(const CalibPairs& pairs, const CalibCameras& cams, const CalibListOfIndexList& seqs)
{
	if (!m_stream.is_open() || pairs.empty() || cams.empty() || seqs.empty()) return;

	size_t imgs = cams.front().GetSize();

	m_stream <<	"	<div class=\"block\">" << endl;
	m_stream << "		<h2>Calibration Graph</h2>" << endl;
	m_stream << "		<h3>Camera Visibility</h3>" << endl;
	m_stream << "		<table cellspacing=\"3\" cellpadding=\"2\">" << endl;
	m_stream << "			<tr><td>&nbsp;</td>";
	for (size_t p = 0; p < cams.size(); p++) m_stream << "<th>CAM<sub>" << p << "</sub></th>"; m_stream << "</tr>" << endl;
	for (size_t p = 0; p < cams.size(); p++)
	{
		m_stream << "			";
		m_stream << "<tr><th>CAM<sub>" << p << "</sub></th><td colspan=\"" << (p+1)  << "\">&nbsp;</td>";
		for (size_t q = p + 1; q < cams.size(); q++)
		{
			const CalibPair* pair = CalibGraph::GetPair(pairs, p, q, cams.size());
			m_stream << "<td class=\"" << (pair->IsEnabled() ? "marked" : "disabled") << "\">" << pair->GetSize() << "</td>";
		}
		m_stream << "</tr>" << endl;
	}
	m_stream << "		</table>" << endl;
	m_stream << "		<h3>Initialisation Errors</h3>" << endl;
	m_stream << "		<table cellspacing=\"3\" cellpadding=\"2\">" << endl;
	m_stream << "			<tr><td>&nbsp;</td>";
	for (size_t p = 0; p < cams.size(); p++) m_stream << "<th>CAM<sub>" << p << "</sub></th>"; m_stream << "</tr>" << endl;
	for (size_t p = 0; p < cams.size(); p++)
	{
		m_stream << "			";
		m_stream << "<tr><th>CAM<sub>" << p << "</sub></th><td colspan=\"" << (p+1)  << "\">&nbsp;</td>";
		for (size_t q = p + 1; q < cams.size(); q++)
		{
			const CalibPair* pair = CalibGraph::GetPair(pairs, p, q, cams.size());
			m_stream << "<td class=\"" << (pair->IsCalibrated() ? "marked" : "disabled") << "\">" << fixed << setprecision(3) << setw(6) << pair->GetRpe() << "</td>";
		}
		m_stream << "</tr>" << endl;
	}
	m_stream << "		</table>" << endl;
	m_stream << "		<h3>Image Visibility</h3>" << endl;
	m_stream << "		<table cellspacing=\"3\" cellpadding=\"2\">" << endl;
	m_stream << "			<tr><td>&nbsp;</td>";
	for (size_t cam = 0; cam < cams.size(); cam++) m_stream << "<th>CAM<sub>" << cam << "</sub></th>"; m_stream << "</tr>" << endl;
	for (size_t img = 0; img < imgs; img++)
	{
		m_stream << "			";
		m_stream << "<tr><th>IMG<sub>" << img << "</sub></th>";
		for (size_t cam = 0; cam < cams.size(); cam++)
		{
			m_stream << "<td class=\"" << (cams[cam].GetData(img).IsOkay() ? "marked" : "disabled") << "\">&nbsp;</td>";
		}
		m_stream << "</tr>" << endl;
	}
	m_stream << "		</table>" << endl;
	m_stream << "		<h3>Traversal Path</h3>" << endl;
	m_stream << "		<ul>" << endl;
	for (size_t p = 0; p < cams.size(); p++)
	{
		m_stream << "			<li><strong>CAM<sub>" << p << "</sub></strong>";
		for (CalibIndexList::const_iterator itr = seqs[p].begin(); itr != seqs[p].end(); itr++)
		{
			m_stream << "&nbsp;-&gt;&nbsp;CAM<sub>" << *itr << "</sub>";
		}
		m_stream << "</li>" << endl;;
	}
	m_stream << "		</ul>" << endl;
	m_stream <<	"	</div>" << endl;
}

void CalibReport::WriteParams(const CalibBundleParams& params, const CalibBundleParams& sigmas)
{
	if (!m_stream.is_open()) return;

	vector<string> labels;
	labels.push_back("f<sub>u</sub>");
	labels.push_back("f<sub>v</sub>");
	labels.push_back("u<sub>c</sub>");
	labels.push_back("v<sub>c</sub>");
	labels.push_back("&kappa;<sub>1</sub>");
	labels.push_back("&kappa;<sub>2</sub>");
	labels.push_back("p<sub>1</sub>");
	labels.push_back("p<sub>2</sub>");
	labels.push_back("&kappa;<sub>3</sub>");
	labels.push_back("r<sub>x</sub>");
	labels.push_back("r<sub>y</sub>");
	labels.push_back("r<sub>z</sub>");
	labels.push_back("t<sub>x</sub>");
	labels.push_back("t<sub>y</sub>");
	labels.push_back("t<sub>z</sub>");

	assert(labels.size() == CalibIntrinsics::NumParameters + CalibExtrinsics::NumParameters);

	m_stream << "	<div class=\"block\">" << endl;
	m_stream << "		<h2>Calibrated Parameters</h2>" << endl;
	m_stream << "		<h3>Intrinsic Parameters</h3>" << endl;
	m_stream << "		<table cellspacing=\"3\" cellpadding=\"2\" class=\"params\">" << endl;
	m_stream << "			<tr><th>&nbsp;</th>";
	for (vector<string>::iterator label = labels.begin(); label != labels.begin() + CalibIntrinsics::NumParameters; label++)
	{
		m_stream << "<th>" << *label << "</th>";
	}
	m_stream << "</tr>" << endl;
	for (size_t cam = 0; cam < params.Intrinsics.size(); cam++)
	{
		vector<double> v = params.Intrinsics[cam].ToVector();
		vector<double> e = sigmas.Intrinsics[cam].ToVector();

		m_stream << "			<tr><th>CAM<sub>" << cam << "</sub></th>";
		for (size_t i = 0; i < v.size(); i++)
		{
			m_stream << "<td align=\"right\">" << fixed << setprecision(2) << v[i];
			m_stream << "<sub>&plusmn;" << fixed << setprecision(2) << e[i]  << "</sub></td>";
		}
		m_stream << "</tr>" << endl;
	}
	m_stream << "		</table>" << endl;
	m_stream << "		<h3>Extrinsic Parameters</h3>" << endl;
	m_stream << "		<table cellspacing=\"3\" cellpadding=\"2\" class=\"params\">" << endl;
	m_stream << "			<tr><th>&nbsp;</th>";
	for (vector<string>::iterator label = labels.begin() + CalibIntrinsics::NumParameters; label != labels.end(); label++)
	{
		m_stream << "<th>" << *label << "</th>";
	}
	m_stream << "</tr>" << endl;
	for (size_t cam = 0; cam < params.Intrinsics.size(); cam++)
	{
		vector<double> v = params.Extrinsics[cam].ToVector();
		vector<double> e = sigmas.Extrinsics[cam].ToVector();

		m_stream << "			<tr><th>CAM<sub>" << cam << "</sub></th>";
		for (size_t i = 0; i < v.size(); i++)
		{
			m_stream << "<td align=\"right\">" << fixed << setprecision(2) << v[i];
			m_stream << "<sub>&plusmn;" << fixed << setprecision(2) << e[i]  << "</sub></td>";
		}
		m_stream << "</tr>" << endl;
	}
	m_stream << "		</table>" << endl;
	m_stream << "		<h3>Target Poses</h3>" << endl;
	m_stream << "		<table cellspacing=\"3\" cellpadding=\"2\" class=\"params\">" << endl;
	m_stream << "			<tr><th>&nbsp;</th>";
	for (vector<string>::iterator label = labels.begin() + CalibIntrinsics::NumParameters; label != labels.end(); label++)
	{
		m_stream << "<th>" << *label << "</th>";
	}
	m_stream << "</tr>" << endl;
	for (size_t img = 0; img < params.ImagePoses.size(); img++)
	{
		vector<double> v = params.ImagePoses[img].ToVector();
		vector<double> e = sigmas.ImagePoses[img].ToVector();

		m_stream << "			<tr><th>IMG<sub>" << img << "</sub></th>";
		for (size_t i = 0; i < v.size(); i++)
		{
			m_stream << "<td align=\"right\">" << fixed << setprecision(2) << v[i];
			m_stream << "<sub>&plusmn;" << fixed << setprecision(2) << e[i]  << "</sub></td>";
		}
		m_stream << "</tr>" << endl;
	}
	m_stream <<	"		</table>" << endl;
	m_stream << "	</div>" << endl;
}

void CalibReport::WriteOptimisationPlot(const CalibOptimState& state)
{
	if (!m_stream.is_open() || state.errors.empty()) return;

	CalibGnuPlot plot(m_outPlotPath, "errors", "history of optimisation error", Size(800,256));
	plot.DataStream << left << setfill(' ') << setw(8) << "#itr" << setw(16) << "rmse" << endl;

	for (size_t i = 0; i < state.errors.size(); i++)
	{
		plot.DataStream << setfill(' ') << setw(8) << i;
		plot.DataStream << setfill(' ') << setw(16) << setprecision(5) << scientific << state.errors[i] << endl;
	}

	plot.ScriptStream << "set grid xtics ytics" << endl;
	plot.ScriptStream << "set title 'Error Plot'" << endl;
	plot.ScriptStream << "set xlabel \"Iteration\"" << endl;
	plot.ScriptStream << "set ylabel \"Error (rmse)\"" << endl;
	plot.ScriptStream << "plot data using 1:2 with linespoints linetype 3 linewidth 1 pointtype 5 pointsize 1 notitle" << endl;
	plot.Render();

	m_stream << "	<div class=\"block\">" << endl;
	m_stream << "		<h2>Optimisation Plot</h2>" << endl;
	m_stream << "		<h3>Error Convergence</h3>" << endl;
	m_stream << "		<img src=\"" << OutPlotDirName << "/" << plot.ImageName << "\" />" << endl;
	m_stream << "		<h3>Time Elapsed</h3>" << endl;
	m_stream << "		<table cellspacing=\"3\" cellpadding=\"2\">" << endl;
	m_stream << "			<tr><th>t<sub>sum</sub></th><th>t<sub>diff</sub></th><th>t<sub>solve</sub></th></tr>" << endl;
	m_stream << "			<tr><td>" << (state.tSum / CLOCKS_PER_SEC) << " secs</td><td>" << (state.tDiff / CLOCKS_PER_SEC) << " secs</td><td>" << (state.tSolve / CLOCKS_PER_SEC) << " secs</td></tr>" << endl;
	m_stream << "		</table>" << endl;
	m_stream << "	</div>" << endl;
}

void CalibReport::WriteHessianMatrix(const Mat& H)
{
	if (!m_stream.is_open() || H.empty()) return;
	CalibGnuPlot plot(m_outPlotPath, "hessian", "Hessian matrix", Size(512,512));
	for (size_t i = 0; i < H.rows; i++)
	{
		for (size_t j = 0; j < H.cols; j++)
		{
			double h_ij = H.at<double>(i,j);
			plot.DataStream << setw(16) << setprecision(5) << scientific << (h_ij == 0 ? 0 : log(h_ij));
		}
		plot.DataStream << endl;
	}

	plot.ScriptStream << "set title 'Hessian Matrix (log scale)'" << endl;
	plot.ScriptStream << "set palette grey negative" << endl;
	plot.ScriptStream << "set xrange [-0.5:" << (H.cols - 0.5) << "]" << endl;
	plot.ScriptStream << "set yrange [-0.5:" << (H.cols - 0.5) << "] reverse" << endl;
	plot.ScriptStream << "plot data matrix with image" << endl;
	plot.Render();

	m_stream << "	<div class=\"block\">" << endl;
	m_stream << "		<h2>Hessian Matrix</h2>" << endl;
	m_stream << "		<img src=\"" << OutPlotDirName << "/" << plot.ImageName << "\" />" << endl;
	m_stream << "	</div>" << endl;
}

void CalibReport::WriteCoverageImages(const CalibCameras& cams)
{
	if (!m_stream.is_open() || cams.empty()) return;

	vector<Colour> colormap;
	Point pt = Point(32, 32);

	cout << "Rendering coverage image..";

	m_stream << "	<div class=\"block\">" << endl;
	m_stream << "		<h2>Target Coverage</h2>" << endl;
	m_stream << "		<table cellspacing=\"3\" cellpadding=\"2\">";
	for (CalibCameras::const_iterator camItr = cams.begin(); camItr != cams.end(); camItr++)
	{
		CalibIndexList imageIndices = camItr->GetImageIndices();
		size_t cam = camItr->GetIndex();
		stringstream ss; ss << "CAM #" << cam;
		Mat im;

		if (colormap.empty())
		{
			colormap = makeHsvColourMap(camItr->GetSize());
		}

		for (CalibIndexList::iterator imgIdxItr = imageIndices.begin(); imgIdxItr != imageIndices.end(); imgIdxItr++)
		{
			size_t img = *imgIdxItr;
			CalibData data = camItr->GetData(img);

			if (im.empty()) // first image?
			{
				string imagePath = fullPath(m_inputPath, data.GetFileName());
				im = imread(imagePath);
			}

			if (im.empty())
			{
				break; // missing image
			}
			
			drawLineStripes(im, data.GetCornerPoints(), colormap[img].ToScalar(), true);
		}

		if (im.empty())
		{
			m_stream << "<td><img width=\"32\" height=\"32\" title=\"SOURCE IMAGE NOT FOUND\" /></td>";
			cout << "..x";
		}
		else
		{
			stringstream ssImgSrc, ssImgId;
			ssImgSrc << "C" << cam << ".jpg";

			string writeName = ssImgSrc.str();
			string writePath = fullPath(m_outImgPath, writeName);

			drawLabel(im, ss.str(), pt, m_fontFace, m_fontScale, m_frontColour, m_backColour);
			imwrite(writePath, im);

			m_stream << "<td><img src=\"" << OutImgDirName << "/" << writeName << "\" /></td>";
			cout << ".." << cam;
		}
	}
	m_stream << "</table>" << endl;
	m_stream << "	</div>" << endl;

	cout << "..DONE" << endl;
}

void CalibReport::WriteFusionImage(const CalibCameras& cams, CalibBundleParams& params)
{
	if (!m_stream.is_open() || cams.empty()) return;

	m_stream << "	<div class=\"block\">" << endl;
	m_stream << "		<h2>Fusion</h2>" << endl;

	Mat fused;
	Mat im0;
	size_t cam0;

	cout << "Rendering rectified fusion image..";

	BOOST_FOREACH(const CalibCam& cam, cams)
	{
		string imagePath = fullPath(m_inputPath, cam.GetData(0).GetFileName());
		Mat im = imread(imagePath);

		if (im.empty())
		{
			cout << "..x";
			continue; // missing image
		}

		if (!params.RectifyInitialised())
		{
			params.InitUndistortRectifyMaps(0, 1, im.size());
		}

		im = params.Rectify(cam.GetIndex(), im);

		if (im0.empty())
		{
			im0 = im.clone();
			cam0 = cam.GetIndex();
		}
		else
		{
			stringstream ss;
			ss << "D" << cam.GetIndex() << ".png";
			string disparityPath = fullPath(m_outImgPath, ss.str());

			float dmax = 128;
			float blockSize = 7;
			float P1 = 8 * blockSize * blockSize;
			float P2 = 4 * P1;
			//Ptr<StereoMatcher> matcher = StereoSGBM::create(0, dmax, blockSize, P1, P2, 1, 63, 15, 100, 32);
			Ptr<StereoMatcher> matcher = StereoBM::create(dmax, blockSize);
			Mat x, y;

			Mat grey0, grey1;
			if (im0.channels() == 3) cvtColor(im0, grey0, COLOR_BGR2GRAY);
			else grey0 = im0;

			if (im.channels() == 3) cvtColor(im, grey1, COLOR_BGR2GRAY);
			else grey1 = im;
			
			matcher->compute(grey0, grey1, x);

			imwrite(disparityPath, x);

			x.convertTo(x, CV_32F, 1/16.0f, 0);
			y = Mat::zeros(x.rows, x.cols, x.type());
			
			for (size_t i = 0; i < im.rows; i++)
			{
				for (size_t j = 0; j < im.cols; j++)
				{
					float dx = x.at<float>(i,j);
					x.at<float>(i,j) = dx < 1e-3 ? -1 : (j + dx);
					y.at<float>(i,j) = i;
				}
			}

			remap(im, im, x, y, INTER_NEAREST);
			waitKey(0);
		}

		if (fused.empty())
		{
			fused = Mat::zeros(im0.rows, im.cols, im0.type());
		}

		fused += im / cams.size();
		cout << ".." << cam.GetIndex();
	}
	
	cout << "..DONE" << endl;

	string writeName = "fused.jpg";
	string writePath = fullPath(m_outImgPath, writeName);

	imwrite(writePath, fused);

	m_stream << "		<img src=\"" << OutImgDirName << "/" << writeName << "\" />" << endl;
	m_stream << "	</div>" << endl;
}

void CalibReport::WriteRpePlots(const vector<vector<ImagePointList>>& pts2d)
{
	if (!m_stream.is_open() || pts2d.empty()) return;

	vector<Colour> colormap;

	m_stream << "	<div class=\"block\">" << endl;
	m_stream << "		<h2>RPE Plots</h2>" << endl;
	m_stream << "		<table cellspacing=\"3\" cellpadding=\"2\">";

	for (size_t cam = 0; cam <pts2d.size(); cam++)
	{
		stringstream ss; ss << "rpe" << cam;
		CalibGnuPlot plot(m_outPlotPath, ss.str(), "reprojection errors", Size(320,320));

		double rmse = 0, n = 0;
		for (size_t img = 0; img < pts2d[cam].size(); img++)
		{
			plot.DataStream << "#IMG" << img << endl;
			plot.DataStream << left << setfill(' ') << setw(16) << "#x" << setw(16) << "y" << endl;
			for (ImagePointList::const_iterator pts = pts2d[cam][img].begin(); pts != pts2d[cam][img].end(); pts++)
			{
				plot.DataStream << setfill(' ') << setw(16) << setprecision(5) << scientific << pts->x << pts->y << endl;

				rmse += pts->x*pts->x + pts->y*pts->y;
				n += 2;
			}
			plot.DataStream << endl;
		}

		rmse = sqrt(rmse / n);

		plot.ScriptStream << "set title \"Reprojection Plot of CAM_{" << cam << "} (rmse=" << fixed << setprecision(2) << rmse << ")\"" << endl;
		plot.ScriptStream << "set xlabel \"u\"" << endl;
		plot.ScriptStream << "set ylabel \"v\"" << endl;
		plot.ScriptStream << "set xrange [-5:5]" << endl;
		plot.ScriptStream << "set yrange [-5:5]" << endl;
		plot.ScriptStream << "set grid xtics ytics" << endl;
		plot.ScriptStream << "plot \\" << endl;

		if (colormap.empty())
		{
			colormap = makeHsvColourMap(pts2d[cam].size());
		}

		for (size_t img = 0; img < pts2d[cam].size(); img++)
		{
			bool lastImage = (img == pts2d[cam].size() - 1);
			plot.ScriptStream << "data index \"IMG" << img << "\" using 1:2 with points title \"IMG_{" << img << "}\" pointtype 1 linecolor rgbcolor \"" << toString(colormap[img]) << "\"" << (lastImage ? ";" : ",\\") << endl;
		}

		plot.Render();
		m_stream << "<td><img src=\"" << OutPlotDirName << "/" << plot.ImageName << "\" /></td>";
	}
	m_stream << "</table>" << endl;
	m_stream << "	</div>" << endl;
}

void CalibReport::WriteRpeImages(const CalibCameras& cams, CalibBundleParams& params)
{
	if (!m_stream.is_open() || cams.empty()) return;

	Point pt = Point(32, 32);
	size_t numImages = cams.begin()->GetSize();

	cout << "Rendering RPE images..";

	m_stream << "	<div class=\"block\">" << endl;
	m_stream << "		<h2>Reprojection on Rectified Images</h2>" << endl;

	for (size_t img = 0; img < numImages; img++)
	{
		m_stream << "		<form>" << endl;
		m_stream << "			<div class=\"visCtrl\">IMG<sub>" << img << "</sub></div>" << endl;

		for (CalibCameras::const_iterator camItr = cams.begin(); camItr != cams.end(); camItr++)
		{
			bool disabled = !camItr->GetData(img).IsOkay();
			stringstream ssChkId, ssImgId, ssImgOnClick;

			ssChkId << "chk-i" << img << "c" << camItr->GetIndex();
			ssImgId << "img-i" << img << "c" << camItr->GetIndex();
			ssImgOnClick << " onclick=\"toggleImg('" << ssImgId.str() << "','" << ssChkId.str() << "')\"";

			m_stream << "			<div class=\"visCtrl\">" << endl;
			m_stream << "				<input type=\"checkbox\" name=\"" << ssChkId.str() << "\" id=\"" << ssChkId.str() << "\" " << (disabled ? "disabled" : "checked") << ">" << endl;
			m_stream << "				<label for=\"" << ssChkId.str() << "\"" << (disabled ? "" : ssImgOnClick.str()) << ">CAM<sub>" << camItr->GetIndex() << "</sub></label>" << endl;
			m_stream << "			</div>" << endl;
		}

		m_stream << "		</form>" << endl;
		m_stream << "		<table cellspacing=\"0\" cellpadding=\"0\"><tr>";
		
		for (CalibCameras::const_iterator camItr = cams.begin(); camItr != cams.end(); camItr++)
		{
			size_t cam = camItr->GetIndex();
			CalibData data = camItr->GetData(img);

			if (!data.IsOkay()) continue;

			stringstream ssImgSrc, ssImgId, ssImgTitle, ssRawSrc;
			ssImgSrc << "R" << cam << "." << img << ".jpg";
			ssRawSrc << "U" << cam << "." << img << ".jpg";
			ssImgId << "img-i" << img << "c" << camItr->GetIndex();

			string writeName = ssImgSrc.str();
			string writePath = fullPath(m_outImgPath, writeName);
			string imagePath = fullPath(m_inputPath, data.GetFileName());

			Mat im = imread(imagePath);

			if (im.empty())
			{
				m_stream << "<td><img id=\"" << ssImgId.str() << "\" width=\"32\" height=\"32\" title=\"SOURCE IMAGE NOT FOUND\" /></td>";
				continue; // missing source image
			}

			if (!params.RectifyInitialised())
			{
				params.InitUndistortRectifyMaps(0, 1, im.size());
			}

			Mat rectified = params.Rectify(cam, im); // do collinear rectification

			ImagePointList ip = data.GetImagePoints();
			ImagePointList rp = params.Project(data.GetObjectPoints(), cam, img);
			
			double rpe = 0;

			for (size_t i = 0; i < ip.size(); i++)
			{
				rpe += sqrt(pow(ip[i].x - rp[i].x, 2) + pow(ip[i].y - rp[i].y, 2));
			}

			rpe /= ip.size();

			drawCrosshair(im, ip, m_markerSize * 1.0f, m_colour1);
			drawCrosshair(im, rp, m_markerSize * 0.5f, m_colour2);

			drawEpipolarLines(rectified, 16, m_frontColour);
			drawCrosshair(rectified, params.UndistortPoints(ip, cam), m_markerSize * 1.0f, m_colour1);
			drawCrosshair(rectified, params.UndistortPoints(rp, cam), m_markerSize * 0.5f, m_colour2);

			ssImgTitle << "CAM #" << cam << " IMG #"  << img << " (" << data.GetFileName() << ") RPE=" << fixed << setprecision(3) << setw(6) << rpe << "px";
			drawLabel(im, ssImgTitle.str(), pt, m_fontFace, m_fontScale, m_frontColour, m_backColour);
			
			imwrite(fullPath(m_outImgPath, ssRawSrc.str()), im);
			imwrite(writePath, rectified);
			m_stream << "<td><img src=\"" << OutImgDirName << "/" << writeName << "\" id=\"" << ssImgId.str() << "\" title=\"" << ssImgTitle.str() << "\" /></td>";
		}
		m_stream << "</tr></table><br />" << endl;
		cout << ".." << img;
	}
	m_stream << "	</div>" << endl;

	cout << "..DONE" << endl;
}
*/