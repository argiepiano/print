<?php

/**
 * @file
 * Generates the PDF version using TCPDF.
 *
 * This file is included by the print_pdf_tcpdf module and includes the
 * functions that interface with the TCPDF library.
 *
 * @ingroup print
 */

/**
 * Implements hook_print_pdf_generate().
 */
function print_pdf_tcpdf_print_pdf_generate($html, $meta, $paper_size = NULL, $page_orientation = NULL) {
  global $base_url, $language;

  module_load_include('inc', 'print', 'includes/print');

  $pdf_tool = explode('|', config_get('print_pdf.settings', 'print_pdf_pdf_tool'));
  if (empty($paper_size)) {
    $paper_size = config_get('print_pdf.settings','print_pdf_paper_size');
  }
  if (empty($page_orientation)) {
    $page_orientation = config_get('print_pdf.settings', 'print_pdf_page_orientation');
  }
  $images_via_file = config_get('print_pdf.settings', 'print_pdf_images_via_file');

  if (config_get('print_pdf.settings', 'print_pdf_autoconfig')) {
    $pdf_tool_path = realpath(dirname($pdf_tool[1]));

    if (!defined('K_TCPDF_EXTERNAL_CONFIG')) {
      define('K_TCPDF_EXTERNAL_CONFIG', TRUE);
    }
    if (!defined('K_PATH_MAIN')) {
      define('K_PATH_MAIN', BACKDROP_ROOT);
    }
    if (!defined('K_PATH_URL')) {
      define('K_PATH_URL', $base_url);
    }
    if (!defined('K_PATH_FONTS')) {
      define('K_PATH_FONTS', $pdf_tool_path . '/fonts/');
    }
    if (!defined('K_PATH_CACHE')) {
      define('K_PATH_CACHE', backdrop_realpath('public://print_pdf/print_pdf_tcpdf/cache') . '/');
    }
    if (!defined('K_PATH_IMAGES')) {
      define('K_PATH_IMAGES', '');
    }
    if (!defined('K_BLANK_IMAGE')) {
      define('K_BLANK_IMAGE', $pdf_tool_path . '/images/_blank.png');
    }
    if (!defined('K_CELL_HEIGHT_RATIO')) {
      define('K_CELL_HEIGHT_RATIO', 1.25);
    }
    if (!defined('K_SMALL_RATIO')) {
      define('K_SMALL_RATIO', 2 / 3);
    }
  }

  // Try to use local file access for image files.
  $html = _print_access_images_via_file($html, $images_via_file);

  // Decode HTML entities in image filenames.
  $pattern = "!<img\s[^>]*?src\s*?=\s*?['\"]?{$base_url}[^>]*?>!is";
  $html = preg_replace_callback($pattern, ($matches)[html_entity_decode($matches[0], ENT_QUOTES)], $html);
  // Remove queries from the image URL.
  $pattern = "!(<img\s[^>]*?src\s*?=\s*?['\"]?{$base_url}[^>]*?)(?:%3F|\?)[^\s'\"]+([^>]*?>)!is";
  $html = preg_replace($pattern, '$1$2', $html);

  $tool_path = BACKDROP_ROOT . '/' . $pdf_tool[1];
  if (file_exists($tool_path)) {
    require_once $tool_path;
  }
  else {
    watchdog('print_pdf', 'Configured PDF tool does not exist at path: %path', array('%path' => $tool_path), WATCHDOG_ERROR);
    throw new Exception("Configured PDF tool does not exist, unable to generate PDF.");
  }
  module_load_include('inc', 'print_pdf_tcpdf', 'print_pdf_tcpdf.class');

  $font = array(
    check_plain(config_get('print_pdf.settings', 'print_pdf_font_family')),
    '',
    check_plain(config_get('print_pdf.settings', 'print_pdf_font_size')),
  );
  $orientation = backdrop_strtoupper($page_orientation[0]);

  // Create new PDF document.
  $pdf = new PrintTCPDF($orientation, 'mm', $paper_size, TRUE);

  // Set document information.
  if (isset($meta['name'])) {
    $pdf->SetAuthor(strip_tags($meta['name']));
  }
  $pdf->SetCreator(config_get('system.core', 'site_name'));
  $pdf->SetTitle(html_entity_decode($meta['title'], ENT_QUOTES, 'UTF-8'));
  $pdf->setPDFVersion('1.6');
  $pdf->setFontSubsetting(config_get('print_pdf.settings', 'print_pdf_font_subsetting'));
  $pdf->setTcpdfLink(FALSE);

  if ($language->direction == LANGUAGE_RTL) {
    $pdf->setRTL(TRUE);
  }

  $pdf = theme('print_pdf_tcpdf_header', array(
    'pdf' => $pdf,
    'html' => $html,
    'font' => $font,
  ));
  $pdf = theme('print_pdf_tcpdf_footer', array(
    'pdf' => $pdf,
    'html' => $html,
    'font' => $font,
  ));
  $pdf = theme('print_pdf_tcpdf_page', array('pdf' => $pdf));

  // Enable third-party module to alter the pdf object, via
  // hook_print_pdf_tcpdf_alter().
  backdrop_alter('print_pdf_tcpdf', $pdf, $html, $meta);

  // Add a page.
  $pdf->AddPage();

  $pdf = theme('print_pdf_tcpdf_content', array(
    'pdf' => $pdf,
    'html' => $html,
    'font' => $font,
  ));

  // Reset pointer to the last page.
  $pdf->lastPage();

  // Try to recover from any warning/error.
  ob_clean();

  return $pdf = $pdf->Output('', 'S');
}

/**
 * Format the TCPDF header.
 *
 * @param array $vars
 *   An associative array containing:
 *   - $pdf: current TCPDF object
 *   - $html: contents of the body of the HTML from the original node
 *   - $font: array with the font definition (font name, styles and size)
 *
 * @return object
 *   modified PDF object
 *
 * @ingroup themeable
 * @ingroup print_themeable
 */
function theme_print_pdf_tcpdf_header($vars) {
  global $base_url;

  $pdf = $vars['pdf'];
  preg_match('!<div class="print-logo">(.*?)</div>!si', $vars['html'], $tpl_logo);
  preg_match('!<title>(.*?)</title>!si', $vars['html'], $tpl_title);
  preg_match('!<div class="print-site_name">(.*?)</div>!si', $vars['html'], $tpl_site_name);

  $ratio = 0;
  $logo = '';
  if (isset($tpl_logo[1]) && preg_match('!src\s*=\s*(?:"(.*?)"|\'(.*?)\'|([^\s]*))!i', $tpl_logo[1], $logo_url)) {
    $logo = $logo_url[1];

    // Make logo relative again.
    $logo = preg_replace("!^$base_url(.*)!sm", BACKDROP_ROOT . '$1', $logo);

    if (!empty($logo)) {
      $size = getimagesize($logo);
      $ratio = $size ? ($size[0] / $size[1]) : 0;
    }
  }

  // Set header font.
  $pdf->setHeaderFont($vars['font']);
  // Set header margin.
  $pdf->setHeaderMargin(5);
  // Set header data.
  $pdf->setHeaderData($logo, 10 * $ratio, html_entity_decode($tpl_title[1], ENT_QUOTES, 'UTF-8'), html_entity_decode(strip_tags($tpl_site_name[1]), ENT_QUOTES, 'UTF-8'));

  return $pdf;
}

/**
 * Format the TCPDF page settings (margins, etc).
 *
 * @param array $vars
 *   An associative array containing:
 *   - $pdf: current TCPDF object.
 *
 * @return object
 *   modified PDF object
 *
 * @ingroup themeable
 * @ingroup print_themeable
 */
function theme_print_pdf_tcpdf_page($vars) {
  $pdf = $vars['pdf'];
  // Set margins.
  $pdf->SetMargins(15, 20, 15);
  // Set auto page breaks.
  $pdf->SetAutoPageBreak(TRUE, 15);
  // Set image scale factor.
  $pdf->setImageScale(1);
  // Set image compression quality.
  $pdf->setJPEGQuality(100);

  return $pdf;
}

/**
 * Format the TCPDF page content.
 *
 * @param array $vars
 *   An associative array containing:
 *   - $pdf: current TCPDF object
 *   - $html: contents of the body of the HTML from the original node
 *   - $font: array with the font definition (font name, styles and size)
 *
 * @return object
 *   modified PDF object
 *
 * @ingroup themeable
 * @ingroup print_themeable
 */
function theme_print_pdf_tcpdf_content($vars) {
  $pdf = $vars['pdf'];
  // Set content font.
  $pdf->setFont($vars['font'][0], $vars['font'][1], $vars['font'][2]);

  // Remove the logo, published, breadcrumb and footer from the main content.
  preg_match('!<body.*?>(.*)</body>!sim', $vars['html'], $matches);
  $pattern = '!(?:<div class="print-(?:logo|site_name|breadcrumb|footer)">.*?</div>|<hr class="print-hr" />)!si';
  $matches[1] = preg_replace($pattern, '', $matches[1]);

  // Make CCK fields look better.
  $matches[1] = preg_replace('!(<div class="field.*?>)\s*!sm', '$1', $matches[1]);
  $matches[1] = preg_replace('!(<div class="field.*?>.*?</div>)\s*!sm', '$1', $matches[1]);
  $matches[1] = preg_replace('!<div( class="field-label.*?>.*?)</div>!sm', '<strong$1</strong>', $matches[1]);

  // Since TCPDF's writeHTML is so bad with <p>, try to make it look nice.
  $matches[1] = preg_replace('!<(?:p(|\s+.*?)/?|/p)>!i', '<br$1 />', $matches[1]);
  $matches[1] = str_replace(array('<div', 'div>'), array('<span', 'span><br />'), $matches[1]);
  do {
    $prev = $matches[1];
    $matches[1] = preg_replace('!(</span>)<br />(\s*?</span><br />)!s', '$1$2', $matches[1]);
  } while ($prev != $matches[1]);

  @$pdf->writeHTML($matches[1]);

  return $pdf;
}

/**
 * Format the TCPDF footer contents.
 *
 * @param array $vars
 *   An associative array containing:
 *   - $pdf: current TCPDF object
 *   - $html: contents of the body of the HTML from the original node
 *   - $font: array with the font definition (font name, styles and size)
 *
 * @return object
 *   modified PDF object
 *
 * @ingroup themeable
 * @ingroup print_themeable
 */
function theme_print_pdf_tcpdf_footer($vars) {
  $pdf = $vars['pdf'];
  preg_match('!<div class="print-footer">(.*?)</div>!si', $vars['html'], $tpl_footer);

  if (isset($tpl_footer[1])) {
    $footer = trim(preg_replace('!</?div[^>]*?>!i', '', $tpl_footer[1]));

    // Set footer font.
    $vars['font'][2] *= 0.8;
    $pdf->setFooterFont($vars['font']);
    // Set footer margin.
    $pdf->SetFooterMargin(10);
    // Set footer data.
    $pdf->setFooterContent($footer);
  }

  return $pdf;
}

/**
 * Format the TCPDF footer layout.
 *
 * @param array $vars
 *   An associative array containing:
 *   - $pdf: current TCPDF object.
 *
 * @return object
 *   modified PDF object
 *
 * @ingroup themeable
 * @ingroup print_themeable
 */
function theme_print_pdf_tcpdf_footer2($vars) {
  $pdf = $vars['pdf'];
  // Position at 1.5 cm from bottom.
  $pdf->writeHTMLCell(0, 15, 15, $pdf->getPageHeight() - 15, $pdf->footer);

  $ormargins = $pdf->getOriginalMargins();
  $pagenumtxt = t('Page !n of !total', array('!n' => $pdf->PageNo(), '!total' => $pdf->getAliasNbPages()));
  // Print page number.
  if ($pdf->getRTL()) {
    $pdf->SetX($ormargins['right']);
    $pdf->Cell(0, 10, $pagenumtxt, 'T', 0, 'L');
  }
  else {
    $pdf->SetX($ormargins['left']);
    $pdf->Cell(0, 10, $pagenumtxt, 'T', 0, 'R');
  }

  return $pdf;
}
