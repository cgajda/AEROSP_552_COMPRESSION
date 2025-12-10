function data = analyze_compression_runs(logFile)
% ANALYZE_COMPRESSION_RUNS Parse AlgoRunSummary events and make plots.
%
%   DATA = ANALYZE_COMPRESSION_RUNS(LOGFILE) parses the given text/CSV
%   export from the F' GDS containing AlgoRunSummary events, returns a
%   struct DATA with fields for algorithm, file sizes, ratios, etc.,
%   and generates several scatter plots.
    if nargin < 1
        logFile = 'algo_run_summary_log.csv'; % change as needed
    end

    fid = fopen(logFile, 'r');
    if fid == -1
        error('Could not open log file: %s', logFile);
    end

    records = struct( ...
        'timestamp',  {}, ...
        'algo',       {}, ...
        'op',         {}, ...
        'file',       {}, ...
        'bytesIn',    [],  ...
        'bytesOut',   [],  ...
        'ratio',      [],  ...
        'dt_us',      [],  ...
        'rss_kib',    []   ...
    );

    pat = [ ...
        'algo=(\w+),\s*' ...
        'op=(\w+),\s*' ...
        'in=([^,]+),\s*' ...
        'bytes_in=(\d+),\s*' ...
        'bytes_out=(\d+),\s*' ...
        'ratio=([0-9eE+\-\.]+),\s*' ...
        'dt_us=(\d+),\s*' ...
        'cpu_x100=\d+,\s*' ...
        'rss_kib=(\d+)' ...
    ];

    % For timestamp at start of line: ^TIMESTAMP,
    tsPat = '^([^,]+)';

    line = fgetl(fid);
    while ischar(line)
        if contains(line, 'RUN_SUMMARY:')
            % Extract timestamp
            tsTok = regexp(line, tsPat, 'tokens', 'once');
            if ~isempty(tsTok)
                tsStr = tsTok{1};
                % Example format: 2025-12-09T19:08:15.985561
                try
                    ts = datetime(tsStr, ...
                        'InputFormat', 'yyyy-MM-dd''T''HH:mm:ss.SSSSSS', ...
                        'TimeZone', 'UTC');
                catch
                    ts = NaT;
                end
            else
                ts = NaT;
            end

            toks = regexp(line, pat, 'tokens', 'once');
            if ~isempty(toks)
                algo      = string(toks{1});
                op        = string(toks{2});
                inFile    = strtrim(string(toks{3}));
                bytesIn   = str2double(toks{4});
                bytesOut  = str2double(toks{5});
                ratio     = str2double(toks{6});
                dt_us     = str2double(toks{7});
                rss_kib   = str2double(toks{8});

                rec.timestamp = ts;
                rec.algo      = algo;
                rec.op        = op;
                rec.file      = inFile;
                rec.bytesIn   = bytesIn;
                rec.bytesOut  = bytesOut;
                rec.ratio     = ratio;
                rec.dt_us     = dt_us;
                rec.rss_kib   = rss_kib;

                records(end+1) = rec; %#ok<AGROW>
            end
        end

        line = fgetl(fid);
    end

    fclose(fid);

    if isempty(records)
        warning('No RUN_SUMMARY lines found in %s', logFile);
        data = struct();
        return;
    end

    data = struct();
    data.timestamp = [records.timestamp]';
    data.algo      = vertcat(records.algo);
    data.op        = vertcat(records.op);
    data.file      = vertcat(records.file);
    data.bytesIn   = [records.bytesIn]';
    data.bytesOut  = [records.bytesOut]';
    data.ratio     = [records.ratio]';
    data.dt_us     = [records.dt_us]';
    data.rss_kib   = [records.rss_kib]';

    % Derived quantities
    data.sizeInMB   = data.bytesIn  / 1e6;     % MB (10^6)
    data.sizeOutMB  = data.bytesOut / 1e6;
    data.dt_s       = data.dt_us    / 1e6;     % seconds
    data.speed_MBps = data.sizeInMB ./ data.dt_s; % compression throughput

    data.efficiency = (1 - data.ratio) ./ data.dt_s;

    % Also export as a table (optional)
    dataTable = table(data.timestamp, data.algo, data.op, data.file, ...
                      data.bytesIn, data.bytesOut, data.sizeInMB, data.sizeOutMB, ...
                      data.ratio, data.dt_s, data.speed_MBps, data.efficiency, ...
                      data.rss_kib, ...
                      'VariableNames', { ...
                        'Timestamp', 'Algo', 'Op', 'File', ...
                        'BytesIn', 'BytesOut', 'SizeInMB', 'SizeOutMB', ...
                        'Ratio', 'Time_s', 'Speed_MBps', 'Efficiency', ...
                        'RSS_KiB'});
    data.table = dataTable;
    make_plots(data);
end

function make_plots(data)

set(groot, 'DefaultAxesFontSize', 18);     % axis labels & ticks
set(groot, 'DefaultTextFontSize', 20);     % titles, annotations
set(groot, 'DefaultLineLineWidth', 2);     % thicker lines & fit curves
    algos  = unique(data.algo);
    colors = lines(numel(algos));  % distinct colors per algorithm

    markerSize = 80;   % bigger points
    lineWidth  = 1.8;  % thicker lines

    fit_and_plot = @(x,y,mask,col) ...
        local_fit_and_plot(x,y,mask,col,lineWidth);

    %Plot 1: Original vs Compressed Size
    figure('Name', 'Original vs Compressed Size');
    tiledlayout(1,1);
    ax1 = nexttile;
    hold(ax1, 'on');

    fitStats1 = cell(numel(algos),1);

    for i = 1:numel(algos)
        mask = data.algo == algos(i) & data.op == "COMPRESS";
        x = data.sizeInMB(mask);
        y = data.sizeOutMB(mask);

        if isempty(x)
            continue;
        end

        scatter(ax1, x, y, ...
            markerSize, ...
            'o', ...
            'MarkerEdgeColor', colors(i,:), ...
            'MarkerFaceColor', colors(i,:), ...
            'LineWidth', 1.3, ...
            'DisplayName', char(algos(i)));

        % Fit line for this algorithm
        fitStats1{i} = fit_and_plot(data.sizeInMB, data.sizeOutMB, mask, colors(i,:));
    end

    % y = x reference
    xMax = max(data.sizeInMB(data.op=="COMPRESS"));
    plot(ax1, [0 xMax*1.05], [0 xMax*1.05], 'k--', ...
        'LineWidth', 1.2, 'DisplayName', 'y = x');

    xlabel(ax1, 'Original Size [MB]');
    ylabel(ax1, 'Compressed Size [MB]');
    title(ax1, 'Original vs Compressed Size');
    grid(ax1, 'on');
    axis(ax1, 'tight');

    annotate_fits(ax1, fitStats1, algos, colors);

    %Plot 2: Compression Ratio vs Original Size
    figure('Name', 'Compression Ratio vs Original Size');
    tiledlayout(1,1);
    ax2 = nexttile;
    hold(ax2, 'on');

    fitStats2 = cell(numel(algos),1);

    for i = 1:numel(algos)
        mask = data.algo == algos(i) & data.op == "COMPRESS";
        x = data.sizeInMB(mask);
        y = data.ratio(mask);

        if isempty(x)
            continue;
        end

        scatter(ax2, x, y, ...
            markerSize, ...
            'o', ...
            'MarkerEdgeColor', colors(i,:), ...
            'MarkerFaceColor', colors(i,:), ...
            'LineWidth', 1.3, ...
            'DisplayName', char(algos(i)));

        fitStats2{i} = fit_and_plot(data.sizeInMB, data.ratio, mask, colors(i,:));
    end

    xlabel(ax2, 'Original Size [MB]');
    ylabel(ax2, 'Compression Ratio (bytes_{out} / bytes_{in})');
    title(ax2, 'Compression Ratio vs Original Size');
    grid(ax2, 'on');
    axis(ax2, 'tight');

    annotate_fits(ax2, fitStats2, algos, colors);

    % Plot 3: Execution Time vs Original Size
    figure('Name', 'Execution Time vs Original Size');
    tiledlayout(1,1);
    ax3 = nexttile;
    hold(ax3, 'on');

    fitStats3 = cell(numel(algos),1);

    for i = 1:numel(algos)
        mask = data.algo == algos(i) & data.op == "COMPRESS";
        x = data.sizeInMB(mask);
        y = data.dt_s(mask);

        if isempty(x)
            continue;
        end

        scatter(ax3, x, y, ...
            markerSize, ...
            'o', ...
            'MarkerEdgeColor', colors(i,:), ...
            'MarkerFaceColor', colors(i,:), ...
            'LineWidth', 1.3, ...
            'DisplayName', char(algos(i)));

        fitStats3{i} = fit_and_plot(data.sizeInMB, data.dt_s, mask, colors(i,:));
    end

    xlabel(ax3, 'Original Size [MB]');
    ylabel(ax3, 'Execution Time [s]');
    title(ax3, 'Execution Time vs Original Size');
    grid(ax3, 'on');
    axis(ax3, 'tight');

    annotate_fits(ax3, fitStats3, algos, colors);

    %Plot 4: Speed–Quality Metric
    figure('Name', 'Speed-Quality Metric');
    tiledlayout(1,1);
    ax4 = nexttile;
    hold(ax4, 'on');

    fitStats4 = cell(numel(algos),1);

    for i = 1:numel(algos)
        mask = data.algo == algos(i) & data.op == "COMPRESS";
        x = data.sizeInMB(mask);
        y = data.efficiency(mask);

        if isempty(x)
            continue;
        end

        scatter(ax4, x, y, ...
            markerSize, ...
            'o', ...
            'MarkerEdgeColor', colors(i,:), ...
            'MarkerFaceColor', colors(i,:), ...
            'LineWidth', 1.3, ...
            'DisplayName', char(algos(i)));

        fitStats4{i} = fit_and_plot(data.sizeInMB, data.efficiency, mask, colors(i,:));
    end

    xlabel(ax4, 'Original Size [MB]');
    ylabel(ax4, 'Efficiency ( (1 - ratio) / time [1/s] )');
    title(ax4, 'Combined Speed-Quality Metric');
    grid(ax4, 'on');
    axis(ax4, 'tight');

    annotate_fits(ax4, fitStats4, algos, colors);
end

function stats = local_fit_and_plot(xAll, yAll, mask, col, lineWidth)
    stats = struct('valid', false, 'p', [], 'R2', NaN);
    x = xAll(mask);
    y = yAll(mask);


    if numel(x) < 2
        return;
    end

    p = polyfit(x, y, 1);
    % R^2 computation
    yHat = polyval(p, x);
    yMean = mean(y);
    SST = sum((y - yMean).^2);
    SSE = sum((y - yHat).^2);
    if SST > 0
        R2 = 1 - SSE / SST;
    else
        R2 = NaN;
    end

    xFit = linspace(min(x), max(x), 100);
    yFit = polyval(p, xFit);

    plot(xFit, yFit, '-', ...
        'Color', col, ...
        'LineWidth', lineWidth, ...
        'HandleVisibility', 'off'); % don't clutter legend

    stats.valid = true;
    stats.p     = p;
    stats.R2    = R2;
end

function annotate_fits(ax, fitStats, algos, colors)
    axes(ax); 
    xl = xlim(ax);
    yl = ylim(ax);
    xSpan = xl(2) - xl(1);
    ySpan = yl(2) - yl(1);

    x0 = xl(1) + 0.02 * xSpan;
    y0 = yl(2) - 0.05 * ySpan;
    dy = 0.05 * ySpan;

    for i = 1:numel(algos)
        s = fitStats{i};
        if ~isfield(s, 'valid') || ~s.valid
            continue;
        end
        p  = s.p;
        R2 = s.R2;

        txt = sprintf('%s: y = %.3g x %+.3g, R^2 = %.3f', ...
                      char(algos(i)), p(1), p(2), R2);

        text(x0, y0 - (i-1)*dy, txt, ...
             'Color', colors(i,:), ...
             'FontSize', 9, ...
             'FontWeight', 'bold', ...
             'Interpreter', 'none'); % avoid underscore issues
    end
end
