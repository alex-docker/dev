# -*- coding: utf-8 -*-
from __future__ import absolute_import

from django.utils.html import mark_safe
from django.utils.translation import ugettext

from .models import IndexInstanceNode

FOLDER_W_DOCUMENTS = u'folder_page'
FOLDER_ICON = u'folder'


def index_instance_item_link(index_instance_item):
    if isinstance(index_instance_item, IndexInstanceNode):
        if index_instance_item.index_template_node.link_documents:
            icon = FOLDER_W_DOCUMENTS
        else:
            icon = FOLDER_ICON
    else:
        icon = u''
    icon_template = u'<span class="famfam active famfam-%s"></span>' % icon if icon else u''
    return mark_safe('%(icon_template)s<a href="%(url)s">%(text)s</a>' % {
        'url': index_instance_item.get_absolute_url(),
        'icon_template': icon_template,
        'text': index_instance_item
    })


def get_instance_link(index_instance_node, text=None, simple=False):
    """
    Return an HTML anchor to an index instance
    """

    if simple:
        # Just display the instance's value or overrided text, no
        # HTML anchor
        template = u'%(value)s'
    else:
        template = u'<a href="%(url)s">%(value)s</a>'

    return template % {
        'url': index_instance_node.get_absolute_url(),
        'value': text if text else (index_instance_node if index_instance_node.parent else index_instance_node.index_template_node.index)
    }


def get_breadcrumbs(index_instance, simple=False, single_link=False, include_count=False):
    """
    Return a joined string of HTML anchors to every index instance's
    parent from the root of the tree to the index instance
    """
    result = []
    if single_link:
        # Return the entire breadcrumb path as a single HTML anchor
        simple = True

    for instance in index_instance.get_ancestors():
        result.append(get_instance_link(instance, simple=simple))

    result.append(get_instance_link(index_instance, simple=simple))

    output = []

    if include_count:
        output.append(u'(%d)' % index_instance.documents.count())

    if single_link:
        # Return the entire breadcrumb path as a single HTML anchor
        output.insert(0, get_instance_link(index_instance_node=index_instance, text=(u' / '.join(result))))
        return mark_safe(u' '.join(output))
    else:
        output.insert(0, u' / '.join(result))
        return mark_safe(u' '.join(output))


def node_level(x):
    """
    Render an indented tree like output for a specific node
    """
    return mark_safe(
        u''.join(
            [
                u'&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;' * (getattr(x, x._mptt_meta.level_attr) - 1),
                u'' if x.is_root_node() else u'',
                ugettext('root') if x.is_root_node() else unicode(x)
            ]
        )
    )
