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
function limerick_migrate_collection_form($form, &$form_state) {
  $form = array();
  $form['source_dir'] = array(
    '#type' => 'textfield',
    '#title' => t("Collection Directory"),
    '#description' => t('Full path to collection directory'),
  );
  $form['namespace'] = array(
    '#type' => 'textfield',
    '#title' => t("Namespace"),
    '#description' => t('Namespace for created collection'),
  );
  $form['collection'] = array(
    '#type' => 'textfield',
    '#title' => t("Collection"),
    '#description' => t('PID of collection to which nested collection is being added'),
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t("Ingest Collection"),
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
function limerick_migrate_collection_form_validate($form, &$form_state) {
  if (!is_dir($form_state['values']['source_dir'])) {
    form_set_error('source_dir', t('Not a valid path'));
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
function limerick_migrate_collection_form_submit($form, &$form_state) {
  $namespace = $form_state['values']['namespace'];
  $collection_pid = $form_state['values']['collection'];
  $source_path = $form_state['values']['source_dir'];
  $dirs = array();
  $files = array();
  foreach ($iterator = new RecursiveIteratorIterator(
  new RecursiveDirectoryIterator($source_path, RecursiveDirectoryIterator::SKIP_DOTS), RecursiveIteratorIterator::SELF_FIRST) as $item) {
    $subpath = $iterator->getSubPathName();
    if ($item->isDir()) {
      $dirs[] = $subpath;
    }
    else {
      $ext = pathinfo($subpath);
      if (in_array($ext, array('tif', 'tiff'))) {
        $files[] = $subpath;
      }
    }
  }
  $batch = array(
    'title' => t('Adding Collection'),
    'operations' => array(),
    'file' => drupal_get_path('module', 'limerick') . '/includes/limerick_collection.inc',
    'progress_message' => t('@current of @total operations completed.'),
  );

  foreach ($dirs as $dir) {
    $batch['operations'][] = array('limerick_create_dir', array(
        'dir' => $dir,
        'namespace' => $namespace,
        'collection_pid' => $collection_pid,
      ),
    );
  }
  foreach ($files as $file) {
    $batch['operations'][] = array('limerick_create_collection_file', array(
        'file' => $file,
        'namespace' => $namespace,
        'collection_pid' => $collection_pid,
        'source_path' => $source_path,
      ),
    );
  }
  batch_set($batch);
  batch_process();
}

/**
 * Creates directory object.
 *
 * @global user $user
 *
 * @param string $dir
 *   Dir anme
 * @param string $namespace
 *   Namespace for created object
 * @param string $collection_pid
 *   PID of parent collection
 */
function limerick_create_dir($dir, $namespace, $collection_pid) {
  global $user;
  $dir_parts = explode('/', $dir);
  $dir_pid = "$namespace:" . implode('-', $dir_parts);
  if (islandora_object_load($dir_pid)) {
    return;
  }
  $label = array_pop($dir_parts);
  if (!$dir_parts) {
    $member_of = $collection_pid;
  }
  else {
    $member_of = $namespace . ':' . implode('-', $dir_parts);
  }
  $tuque = new IslandoraTuque();
  $object = $tuque->repository->constructObject($dir_pid);
  $object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOf', $member_of);
  $object->relationships->add(FEDORA_MODEL_URI, 'hasModel', 'islandora:collectionCModel');
  $object->label = $label;
  $object->owner = $user->name;
  islandora_add_object($object);
}

/**
 * Creates object from file.
 *
 * @global user $user
 *
 * @param string $file
 *   File name
 * @param string $namespace
 *   Namespace of ingested object
 * @param syring $collection_pid
 *   PID of collection
 * @param string $source_path
 *   File Path
 */
function limerick_create_collection_file($file, $namespace, $collection_pid, $source_path) {
  module_load_include('inc', 'limerick', 'includes/utilities');

  global $user;
  $file_parts = explode('/', $file);
  $tuque = new IslandoraTuque();
  $label = pathinfo($file, PATHINFO_FILENAME);
  array_pop($file_parts);
  if (!$file_parts) {
    $member_of = $collection_pid;
  }
  else {
    $member_of = $namespace . ':' . implode('-', $file_parts);
  }
  $file_path = "$source_path/$file";
  $file_path_parts = explode('/', $file_path);
  array_pop($file_path_parts);
  $mods_path = implode('/', $file_path_paths) . '/MODS/' . "{$label}.xml";
  $object = $tuque->repository->constructObject($namespace);
  $object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOf', $member_of);
  $object->relationships->add(FEDORA_MODEL_URI, 'hasModel', 'islandora:sp_large_image_cmodel');
  $object->label = $label;
  $object->owner = $user->name;
  $ds = $object->constructDatastream('OBJ', 'M');
  $ds->setContentFromFile($file_path, FALSE);
  $ds->label = 'OBJ';
  $ds->mimetype = "image/tiff";
  $object->ingestDatastream($ds);
  limerick_add_mods($object, $mods_path);
  islandora_add_object($object);
}
