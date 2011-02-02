<?php

require_once "startup.php";

$projectName = "$[/plugins/Flowviz/projectName]";
$storagePath = "/projects/" . $projectName;

if (isset($_POST["formId"])) {
    if (getPostData("action") != "Cancel") {

        $dotPath = getPostData("dotPath");

        $updateManager = new UpdateManager;

        $update = $updateManager->createUpdate("setProperty");
        $update->addItem("propertyName", "$storagePath/dotPath");
        $update->addItem("value", $dotPath);

        $updateManager->handleUpdates();
    }
    header("Location: " . nonHtmlUrl2("/commander/pages/EC-PluginManager/pluginmanager?s=Administration&ss=Plugins", array()));
    exit();
}

$queryManager = new QueryManager;

$dotPathQuery = $queryManager->addQuery("getProperty",
        array("propertyName", "$storagePath/dotPath"));
$dotPathQuery->setProperty("suppressErrors", array("NoSuchProperty"));
$queryManager->handleQueries();

$dotPath = new Entry(array("initialValue" =>
        $dotPathQuery->getResponse()->get("value")));

$form = new Form(array(
    "id"              => "selectSearchFilter",
    "noPostAction"    => true,
    "elements"        => array(
        ecgettext("Path to dot executable:"),    "dotPath",     $dotPath
    )));

$title = "Flowviz";
$actionTitle = "configuration";

$page = new Page(
    ecgettext("$title - $actionTitle"),
    new Header(
        array(
            "id"      => "pageHeader",
            "class"   => "pageHeader",
            "title"   => ecgettext($title),
            "title2"  => ecgettext($actionTitle)
        )
    ),
    new SubSection($form)
);

$page->show();
?>
