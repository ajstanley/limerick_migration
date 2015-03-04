<?php

/**
 * @file
 * Forms to migrate books.
 */

/**
 * Form to define parameters for moving entire collections.
 *
 * @param array $form
 *   Drupal Form
 * @param array $form_state
 *   Drupal from state
 *
 * @return array
 *   Drupal form
 */
function limerick_migrate_postcard_form($form, &$form_state) {
  $form = array();
  $form['source_dir'] = array(
    '#type' => 'textfield',
    '#title' => t("Postcard Directory"),
    '#description' => t('Full path to postcard directory'),
  );
  $form['namespace'] = array(
    '#type' => 'textfield',
    '#title' => t("Namespace"),
    '#description' => t('Namespace for created book'),
  );
  $form['collection'] = array(
    '#type' => 'textfield',
    '#title' => t("Collection"),
    '#description' => t('PID of collection to which postcards are being added'),
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t("Ingest Postcards"),
  );
  return $form;
}

/**
 * Validation handler.
 *
 * @param array $form
 *   Drupal Form
 * @param array $form_state
 *   Drupal form state
 */
function limerick_migrate_postcard_form_validate($form, &$form_state) {
  if (!is_dir($form_state['values']['source_dir'])) {
    form_set_error('source_dir', t('Not a valid file path.'));
  }
  if (!islandora_object_load($form_state['values']['collection'])) {
    form_set_error('collection', t('Collection does not exist.'));
  }
}

/**
 * Submit handler.
 *
 * @param array $form
 *   Drupal Form
 * @param array $form_state
 *   Drupal form state
 */
function limerick_migrate_postcard_form_submit($form, &$form_state) {
  $namespace = $form_state['values']['namespace'];
  $collection_pid = $form_state['values']['collection'];
  $source_path = $form_state['values']['source_dir'];
  $dh = opendir($source_path);
  while ($filename = readdir($dh)) {
    $ext = pathinfo($filename, PATHINFO_EXTENSION);
    $file = pathinfo($filename, PATHINFO_FILENAME);
    if (in_array($ext, array('tiff', 'tif'))) {
      if (substr($file, strlen($file) - 1) == 'A') {
        $tiffs[] = "$source_path/$filename";
      }
    }
  }
  sort($tiffs);
  $batch = array(
    'title' => t('Adding Postcards'),
    'operations' => array(),
    'file' => drupal_get_path('module', 'limerick') . '/includes/limerick_postcard.inc',
    'progress_message' => t('@current of @total operations.'),
  );
  foreach ($tiffs as $tiff) {
    $batch['operations'][] = array('limerick_create_postcard', array(
        'tiff' => $tiff,
        'collection_pid' => $collection_pid,
        'namespace' => $namespace,
      ),
    );
  }
  batch_set($batch);
  batch_process();
}

/**
 * Creates compound object related postcard objects.
 *
 * @global type $user
 *
 * @param string $recto_path
 *   File path to recto image
 * @param string $collection_pid
 *   Pid of collection
 * @param string $namespace
 *   Namespace for created objects
 */
function limerick_create_postcard($recto_path, $collection_pid, $namespace) {
  module_load_include('inc', 'limerick', 'includes/utilities');
  global $user;
  module_load_include('inc', 'islandora_compound_object', 'includes/manage.form');
  module_load_include('inc', 'islandora_large_image', 'includes/derivatives');
  $verso_path = str_replace("A.tif", 'B.tif', $recto_path);
  $paths = array($recto_path, $verso_path);
  $mods_path = pathinfo($recto_path, PATHINFO_FILENAME) . '/MODS';
  // Build Compound object.
  $tuque = new IslandoraTuque();
  $label = substr(pathinfo($recto_path, PATHINFO_FILENAME), 0, strlen($label) - 1);
  $compound_object = $tuque->repository->constructObject($namespace);
  $compound_object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOf', $collection_pid);
  $compound_object->relationships->add(FEDORA_MODEL_URI, 'hasModel', 'islandora:compoundCModel');
  $compound_object->label = $label;
  $compound_object->owner = $user->name;
  $args = array();
  $args[] = '-quality ' . escapeshellarg(variable_get('imagemagick_quality', 75));
  $args[] = '-resize ' . escapeshellarg("200 x 200");
  $derivative_file = islandora_large_image_imagemagick_convert($recto_path, "temporary://{$label}_TN.jpg", $args);
  islandora_large_image_add_datastream($compound_object, 'TN', $derivative_file, 'image/jpeg', t('Thumbnail'));
  limerick_add_mods($compound_object, "$mods_path/{$label}.xml");
  $compound_object = islandora_add_object($compound_object);
  $objects = array();

  // Add both images as member objects.
  foreach ($paths as $path) {
    $label = pathinfo($path, PATHINFO_FILENAME);
    $object = $tuque->repository->constructObject($namespace);
    $object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOf', $collection_pid);
    $object->relationships->add(FEDORA_MODEL_URI, 'hasModel', 'islandora:sp_large_image_cmodel');
    $object->label = $label;
    $object->owner = $user->name;
    // Add OBJ.
    $ds = $object->constructDatastream('OBJ', 'M');
    $object->ingestDatastream($ds);
    $ds->setContentFromFile($path, FALSE);
    $ds->label = 'OBJ';
    $ds->mimetype = "image/tiff";
    limerick_add_mods($compound_object, "$mods_path/{$label}.xml");
    $objects[] = islandora_add_object($object);
  }

  islandora_compound_object_add_parent($objects, $compound_object->id);
}
