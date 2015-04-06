<?php

/**
 * @file
 * Utility migration functions.
 */

/**
 * Adds MODS datastram to object.
 *
 * @param AbstractObject $object
 *   Book Object
 * @param string $mods_path
 *   Path to MODS file
 */
function limerick_add_mods(&$object, $mods_path) {
  if (is_file($mods_path)) {
    $raw_mods = trim(file_get_contents($mods_path));
    $dom = new DOMDocument();
    $dom->loadXML($raw_mods);
    $xpath = new DomXpath($dom);
    $xpath->registerNamespace('mods', 'http://www.loc.gov/mods/v3');
    $titles = $xpath->query('//mods:mods/mods:titleInfo/mods:title');
    if ($titles->length > 0) {
      $label = $titles->item(0)->nodeValue;
    }
    else {
      $titles = $xpath->query('//mods/titleInfo/title');
      $label = $titles->item(0)->nodeValue;
      $label = html_entity_decode($label);
    }
    if ($label) {
      $object->label = $label;
    }
    $mods = $object->constructDatastream('MODS');
    $mods->label = 'MODS';
    $mods->mimetype = 'text/xml';
    $mods->setContentFromString($raw_mods);
    $object->ingestDatastream($mods);
  }
}

/**
 * Gathers DC information from handles
 *
 * @param url $handle
 *
 * DSpace handle
 * @param AbstractObject $object
 *   Object representing digital asset
 */
function limerick_harvest_dc($handle, $object) {
  $sample_handle = "http://ulir.ul.ie/handle/10344/1007";
  module_load_include('inc', 'islandora', 'includes/dublin_core');
  $current = NULL;
  if ($object['DC']) {
    $current = $object['DC']->content;
  }
  $dc = new DublinCore();
  $dom = new DOMDocument();
  @$dom->loadHTMLFile("$handle?show=full");
  $xpath = new DomXpath($dom);
  $metas = $xpath->query('//meta');
  foreach ($metas as $meta) {
    $name = $meta->getAttribute('name');
    if (substr($name, 0, 3) == 'DC.') {
      $name = str_replace('DC.', 'dc:', $name);
      $value = $meta->getAttribute('content');
      $dc->addElement($name, $value);
    }
  }
  $dc_xml = $dc->asXML();
  if ($object['DC']) {
    $object['DC']->content = $dc_xml;
  }
  else{
    $dc = $object->constructDatastream('DC');
    $dc->label = 'DC';
    $dc->mimetype = 'text/xml';
    $dc->setContentFromString($dc_xml);
    $object->ingestDatastream($dc);
  }
}

function limerick_extract_mods($record_path, $destination_path) {
  $collection = trim(file_get_contents($record_path));
  $dom = new DOMDocument();
  $dom->loadXML($collection);
  $xpath = new DomXpath($dom);
  $xpath->registerNamespace('mods', 'http://www.loc.gov/mods/v3');
  $mods = $xpath->query('//mods:mods');
  foreach ($mods as $record) {
    $mods_dom = new DOMDocument();
    $mods_dom->loadXML($dom->saveXML($record));
    $root = $mods_dom->documentElement;
    $root->appendChild($mods_dom->createAttribute('xmlns:xsi'))->appendChild($mods_dom->createTextNode("http://www.w3.org/2001/XMLSchema-instance"));
    $root->appendChild($mods_dom->createAttribute('xsi:schemaLocation'))->appendChild($mods_dom->createTextNode("http://www.loc.gov/mods/v3 http://www.loc.gov/standards/mods/v3/mods-3-0.xsd"));
    $root->appendChild($mods_dom->createAttribute('xmlns'))->appendChild($mods_dom->createTextNode("http://www.loc.gov/mods/v3"));
    $mods_xpath = new DomXpath($dom);
    $mods_xpath->registerNamespace('mods', 'http://www.loc.gov/mods/v3');
    $identifiers = $xpath->query('//mods:mods/mods:identifier[@type = "uri"]');
    $identifier = $identifiers->item(0);
    $file = $identifier->nodeValue . ".xml";
    $contents = $mods_dom->saveXML();
    file_put_contents("$destination_path/$file", $contents);
  }
}